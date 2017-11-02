#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>		//Input and outputs, printf
#include <regex>
#include <minwinbase.h>
#include <sstream>		//String stream to convert int to string

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

#define MIN_PORT 5000
#define MAX_PORT 5050
#define IP_ADDR_MAXLENGTH 16
#define PORT_MAXLENGTH 4


//Returns error message associated with last encountered WSA Error code
wchar_t* WSAGetLastErrorMessage()
{
	wchar_t *s = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&s, 0, NULL);
	
	return s;
}

bool IsValidIP(char* ipAddress)
{
	std::regex ipRegEx = std::regex("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	return std::regex_match(ipAddress, ipRegEx);
}

void GetServerIPAddress(char*& ipAddress, char*& port)
{
	char tempIPAddr[IP_ADDR_MAXLENGTH];
	do
	{
		printf("Type in server IP address: ");
		gets_s(tempIPAddr);
	} while (!IsValidIP(tempIPAddr));

	strcpy_s(ipAddress, sizeof(tempIPAddr), tempIPAddr);

	int tempPort;
	do
	{
		printf("Choose a port to listen on between 5000-5050: ");
		scanf_s("%d", &tempPort);
	} while (tempPort < MIN_PORT || tempPort > MAX_PORT);

	//Convert int to char*
	std::stringstream ss;
	ss << tempPort;
	strcpy_s(port, 5, ss.str().c_str());
}

int openSocket(char* ipAddress, char* port, SOCKET sock)
{
	struct addrinfo hints, *res = NULL;
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
	if(sock == INVALID_SOCKET)
	{
		printf("%S\n", WSAGetLastErrorMessage());
		freeaddrinfo(res);
		WSACleanup();
		return 1;
	}

	//Bind socket to network address to listen
	iResult = bind(sock, res->ai_addr, (int)res->ai_addrlen);
	if(iResult == SOCKET_ERROR)
	{
		printf("%S\n", WSAGetLastErrorMessage());
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	//Information returned by getaddrinfo is no longer needed
	freeaddrinfo(res);

	return 0;
}

int main() {

	/* Set up winsock - This part is exactly the same for client and server */

	WSADATA wsaData;	//Struct that contains information about the Windows Sockets implementation

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); //MAKEWORD(2,2) Request for version 2.2 of Winsock on the system
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}


	//Create socket to listen to incomming connection requests over IPv4 on TCP protocol
	SOCKET listenSocket = INVALID_SOCKET;

	char* ipAddress = new char[IP_ADDR_MAXLENGTH];
	char* portNumber = new char[PORT_MAXLENGTH];
	GetServerIPAddress(ipAddress, portNumber);

	openSocket(ipAddress, portNumber, listenSocket);

	return 0;
}