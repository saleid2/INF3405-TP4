#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>		//Input and outputs, printf

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

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

	return 0;
}