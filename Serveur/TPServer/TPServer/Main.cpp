#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>		//Input and outputs, printf
#include <regex>
//#include <minwinbase.h> // TODO: Check if this include is actually necessary
#include <sstream>		//String stream to convert int to string

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

#define MIN_PORT 5000
#define MAX_PORT 5050
#define IP_ADDR_MAX_LENGTH 16
#define PORT_LENGTH 4
#define MAX_MSG_LENGTH 200
#define MAX_STORED_MSG 15

// TODO: Determine max username length to use on server & client
#define MAX_USERNAME_LENGTH 100

struct ChildThreadParams
{
	SOCKET sd_;
	std::vector<SOCKET> openedSockets_;
	sockaddr_in sinRemote_;
};

extern DWORD WINAPI EchoHandler(void* ctp);

//Returns error message associated with last encountered WSA Error code
wchar_t* WSAGetLastErrorMessage()
{
	wchar_t* s = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               nullptr, WSAGetLastError(),
	               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	               (LPWSTR)&s, 0, nullptr);

	return s;
}

//Checkcs if IP address is valid
bool IsValidIP(char* ipAddress)
{
	std::regex ipRegEx = std::regex(
		"^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	return regex_match(ipAddress, ipRegEx);
}

//Asks user to input IP and Port and validates the information
void GetServerIPAddress(char*& ipAddress, char*& port)
{
	char tempIPAddr[IP_ADDR_MAX_LENGTH];
	do
	{
		printf("Type in server IP address: ");
		gets_s(tempIPAddr);
	}
	while (!IsValidIP(tempIPAddr));

	strcpy_s(ipAddress, sizeof(tempIPAddr), tempIPAddr);

	int tempPort;
	do
	{
		printf("Choose a port to listen on between 5000-5050: ");
		scanf_s("%d", &tempPort);
	}
	while (tempPort < MIN_PORT || tempPort > MAX_PORT);

	//Convert int to char*
	std::stringstream ss;
	ss << tempPort;
	strcpy_s(port, 5, ss.str().c_str());
}

int openSocket(char* ipAddress, char* port, SOCKET sock)
{
	struct addrinfo hints, *res = nullptr;
	int iResult;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Get server host information
	iResult = getaddrinfo(ipAddress, (PCSTR)port, &hints, &res);
	if (iResult != 0)
	{
		printf("%S\n", WSAGetLastErrorMessage());
		WSACleanup();
		return 1;
	}

	//Open socket
	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock == INVALID_SOCKET)
	{
		printf("%S\n", WSAGetLastErrorMessage());
		freeaddrinfo(res);
		WSACleanup();
		return 1;
	}

	//Bind socket to network address to listen
	iResult = bind(sock, res->ai_addr, (int)res->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("%S\n", WSAGetLastErrorMessage());
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	//Information returned by getaddrinfo is no longer needed
	freeaddrinfo(res);

	//Listens for connections on socket
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("%S\n", WSAGetLastErrorMessage());
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	//Store a vector of all open sockets so that child processes can access and send messages
	std::vector<SOCKET> openSockets;

	// TODO: Test connection and thread creation
	while (true)
	{
		sockaddr_in sinRemote;
		int nAddrSize = sizeof(sinRemote);

		//Create a Socket for accepting incoming requests
		//Accept the connection
		SOCKET sd = accept(sock, (sockaddr*)&sinRemote, &nAddrSize);

		if (sd != INVALID_SOCKET)
		{
			openSockets.push_back(sd);

			ChildThreadParams ctp;
			ctp.sd_ = sd;
			ctp.openedSockets_ = openSockets;
			ctp.sinRemote_ = sinRemote;

			DWORD nThreadID;
			CreateThread(nullptr, 0, EchoHandler, &ctp, 0, &nThreadID);
		}
		else
		{
			printf("%S\n", WSAGetLastErrorMessage());
		}
	}

	return 0;
}

int main()
{
	/* Set up winsock - This part is exactly the same for client and server */

	WSADATA wsaData; //Struct that contains information about the Windows Sockets implementation

	//Initialize WinSock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); //MAKEWORD(2,2) Request for version 2.2 of Winsock on the system

	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}


	//Create socket to listen to incomming connection requests over IPv4 on TCP protocol
	SOCKET listenSocket = INVALID_SOCKET;

	char* ipAddress = new char[IP_ADDR_MAX_LENGTH];
	char* portNumber = new char[PORT_LENGTH];

	GetServerIPAddress(ipAddress, portNumber);

	openSocket(ipAddress, portNumber, listenSocket);

	return 0;
}

DWORD WINAPI EchoHandler(void* ctp_)
{
	ChildThreadParams* ctp = (ChildThreadParams*)ctp_;
	SOCKET sd = ctp->sd_;

	bool disconnectRequested = false;

	char readBuffer[MAX_MSG_LENGTH], username[MAX_USERNAME_LENGTH];
	int readBytes;

	//User's IP: inet_ntoa(ctp->sinRemote_.sin_addr);
	//User's Port: ntohs(ctp->sinRemote_.sin_port);

	//Read Data from client
	do
	{
		readBytes = 0;
		readBytes = recv(sd, readBuffer, MAX_MSG_LENGTH, 0);

		if (readBytes > 0)
		{
			// TODO: Check if message is for login
			// TODO: Check if message is for disconnect
			// TODO: Save and broadcast received message
		}
		else
		{
			printf("%S\n", WSAGetLastErrorMessage());
		}
	}
	while (!disconnectRequested);

	closesocket(sd);

	return 0;
}
