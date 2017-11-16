#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>
#include <iostream>
#include <string>
#include <regex>		
#include <conio.h>		//_gretch() : press any key to continue
#include <thread>		

/*
TO DO : PICK AND IMPLEMENT LOG OUT PROCEDURE (empty message as suggested in email?)
TO DO : MANAGE 200 CHAR MAX
TO DO : SEPARATE NAME AND TEXT FROM INCOMING MESSAGES
TO DO : WHAT HAPPENS TO SENT MESSAGES (APPEAR DOUBLE... MAY HAVE TO MASK CLIENT'S OWN FROM BROADCASTS)
*/

using namespace std;

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

/* Global const */
#define MIN_PORT 5000
#define MAX_PORT 5050
#define MESSAGE_MAX_LENGTH 200	//INUTILE
#define MAX_MSG_BUF_LENGTH 300	//TO DO : Validate with Salim and calculate real max
#define MAX_USERNAME_LENGTH 10	//TO DO : Validate with Salim
#define MAX_PASSWORD_LENGTH 8	//TO DO : Validate with Salim

struct Client
{
	SOCKET socket;
	char receivedMessage[MAX_MSG_BUF_LENGTH];
	string ipAddress;
	string port;
	string username;
	string password;
};

//Headers and separator
const string MSG_TYPE_MSG = "$msg:";
const string MSG_TYPE_LOG = "$log:";
const string MSG_TYPE_LOGIN_OK = "$login_ok:";
const string MSG_TYPE_LOGIN_FAILED = "$login_failed:";
const string MSG_TYPE_LOGOUT = "$quit:";
const string DATA_SEPARATOR = "%__%";

/* Function prototyping */
void requestPortAndIP(Client &client);
void requestLoginInfo(Client &client);
bool validateUsername(string &username);
bool validatePassword(string &password);
bool validateIP(string &ip);
bool validatePort(string &port);
int submitLoginRequest(Client &client);
int processIncomingMessage(Client &client);
bool verifyTooLong(string &message);
bool verifyDisconnectResquest(string &message);
void printMessage(Client &client);
int closeProgramRoutine(Client &client);

int main() {

	// Set up winsock - This part is exactly the same for client and server */

	WSADATA wsaData;	//Structure that contains information about the Windows Sockets implementation

	int iResult;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); //MAKEWORD(2,2) Request for version 2.2 of Winsock on the system
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		printf("Press any key to end the program\n");
		_getch();
		return 1;
	}

	//  addrinfo : Object that contains a sockaddr structure
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	Client client = { INVALID_SOCKET, "", "", "", "", ""};

	requestPortAndIP(client);
	requestLoginInfo(client);

	string ip = client.ipAddress;

	// Resolve the server address and port
	iResult = getaddrinfo((PCSTR)&client.ipAddress, (PCSTR)&client.port, &hints, &result);	//TODO if connection issues, perhaps casts don't work
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		return (closeProgramRoutine(client));
	}

	//Find first valid address
	while ((result != NULL) && (result->ai_family != AF_INET))
		result = result->ai_next;
	
	//If address not found
	if (((result->ai_family != AF_INET || (result == NULL)))) {
		printf("Cannot find address \n");
		return (closeProgramRoutine(client));
	}

	sockaddr_in *address;
	address = (struct sockaddr_in *) result->ai_addr;

	printf("Connecting to port %s of server %s\n", client.port.c_str(), inet_ntoa(address->sin_addr));

	ptr = result;

	// Create a SOCKET for connecting to server
	client.socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

	if (client.socket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		return (closeProgramRoutine(client));
	}

	//Connecting to server
	iResult = connect(client.socket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("Cannot connect to port %s of server %s\n", client.port.c_str(), inet_ntoa(address->sin_addr));
		return (closeProgramRoutine(client));
	}

	cout << "Successful connection so server" << endl;

	if (!submitLoginRequest(client)) {
		printf("Login failed\n");
		return (closeProgramRoutine(client));
	}

	freeaddrinfo(result);	//??
	//thread listening to server
	thread my_thread(processIncomingMessage, client);
	string messageToServer = "";

	while (1)
	{
		getline(cin, messageToServer);
		if (!verifyTooLong(messageToServer)) {
			iResult = send(client.socket, messageToServer.c_str(), strlen(messageToServer.c_str()), 0);

			if (iResult <= 0)
			{
				cout << "send() failed: " << WSAGetLastError() << endl;
				break;
			}
		}
		else if (verifyDisconnectResquest(messageToServer))
			break;
	}

	//Shutdown the connection since no more data will be sent
	my_thread.detach();

	return 0;
}

void requestPortAndIP(Client& client) {

	cout << "Launching chatroom software" << endl;

	do {
		cout << "Please enter the server IP address: ";
		cin >> client.ipAddress;
	} while (!validateIP(client.ipAddress));
	
	do {
		cout << "Please pick a port number between " << MIN_PORT << " and " << MAX_PORT << ": ";
		cin >> client.port;
	} while (!validatePort(client.port));
}

void requestLoginInfo(Client &client) {

	do {
		cout << "Please enter your username: ";
		cin >> client.username;
	} while (!validateUsername(client.username));

	do {
		cout << "Please enter your password: ";
		cin >> client.password;
	} while (!validatePassword(client.password));

}

bool validateUsername(string &username) {
	int length = username.length();
	if (length > MAX_USERNAME_LENGTH){
		cout << "Username must be below " << MAX_USERNAME_LENGTH << " characters." << endl;
		return false;
	}
	return true;
};

bool validatePassword(string &password) {
	if (password.length() > MAX_PASSWORD_LENGTH) {
		cout << "Password must be below " << MAX_PASSWORD_LENGTH << " characters." << endl;
		return false;
	}
	return true;
};

bool validateIP(string& ip) {

	regex ipRegex("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	bool isValidIp = regex_search(ip, ipRegex);

	if (!isValidIp) {
		cout << "The IP address is not valid. Please enter a valid IP address." << endl;
	}

	return (isValidIp);
}

bool validatePort(string& port) {

	regex ipRegex("^50([0-4][0-9]|50)$");

	bool isValidPort = regex_search(port, ipRegex);

	if (!isValidPort) {
		cout << "The port number is not valid. Please enter a valid port number between " << MIN_PORT << " and " << MAX_PORT << "."<< endl;
	}

	return (isValidPort);
}

int submitLoginRequest(Client &client) {
	
	int iResult;
	string loginInfo = MSG_TYPE_LOG + client.username + DATA_SEPARATOR + client.password;
	//Send login info
	iResult = send(client.socket, loginInfo.c_str(), strlen(loginInfo.c_str()), 0);
	//Analyse login response
	recv(client.socket, client.receivedMessage, MAX_MSG_BUF_LENGTH, 0);
	string messageFromServer = client.receivedMessage;

	if (messageFromServer == MSG_TYPE_LOGIN_FAILED)
		requestLoginInfo(client);
	else if (messageFromServer == MSG_TYPE_LOGIN_OK)
		return 1;
	return 0;
}

void printMessage(Client& client) {
	string receivedMessage = client.receivedMessage;
	if (receivedMessage.substr(0, MSG_TYPE_MSG.length()) == MSG_TYPE_MSG) {
		receivedMessage.erase(0, MSG_TYPE_MSG.length());
		cout << "MESSAGE DATA : " << receivedMessage << endl;
	}
	else
		cout << "ERROR : Unknown message header from server" << endl;
};

bool verifyTooLong(string &message) {
	if (message.length() > MESSAGE_MAX_LENGTH) {
		cout << "CHATROOM WARNING: Your message had " << message.length() << "and thus was not sent" <<endl;
		cout << "CHATROOM REMINDER: Max message length is " << MESSAGE_MAX_LENGTH << " and blank messages will disconnect from chat." << endl;
		return true;
	}
	return false;
}

bool verifyDisconnectResquest(string &message) {
	if (message.length() == 0) {
		cout << "CHATROOM REMINDER: You have entered a blank message. Press any key to close program." << endl;
		return true;
	}
	return false;
}

int processIncomingMessage(Client& client)
{
	while (1)
	{
		if (client.socket != 0)
		{
			int iResult = recv(client.socket, client.receivedMessage, MAX_MSG_BUF_LENGTH, 0);

			if (iResult != SOCKET_ERROR)
				printMessage(client);
			else
			{
				cout << "recv() failed: " << WSAGetLastError() << endl;
				break;
			}
		}
	}

	if (WSAGetLastError() == WSAECONNRESET)
		cout << "The server has disconnected" << endl;

	return 0;
}

int closeProgramRoutine(Client &client) {
	closesocket(client.socket);
	WSACleanup();
	printf("Press any key to end the program\n");
	getchar();
	return 1;
}
