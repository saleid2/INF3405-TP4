#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>
#include <iostream>
#include <string>


using namespace std;

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

/* Global const */
#define IPV4_LENGTH 16
#define PORT_MAX_LENGTH 5

/* Function prototyping */

void requestPortAndIP(char* &ipAddress, char* &port);
bool validateIP(string ip);


int main() {

	/* Set up winsock - This part is exactly the same for client and server */

	WSADATA wsaData;	//Structure that contains information about the Windows Sockets implementation

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); //MAKEWORD(2,2) Request for version 2.2 of Winsock on the system
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	//  addringo : Object that contains a sockaddr structure
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		//internet address family (CHANGE TO AF_INET ??)
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char* ipAddress = new char[IPV4_LENGTH];
	char* port = new char[PORT_MAX_LENGTH];

	requestPortAndIP(ipAddress, port);

	// Resolve the server address and port
	iResult = getaddrinfo(ipAddress, port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// TO CONTINUE

	return 0;
}

void requestPortAndIP(char* &ipAddress, char* &port) {

	cout << "Launching chatroom" << endl;

	char tempIP[IPV4_LENGTH];

	do {
		cout << "Please enter the server IP address: ";
		gets_s(tempIP);
	} while (!validateIP(tempIP));
	
	// TO CONTINUE

}

bool validateIP(string ip) {
	//TO DO
	return false;
}