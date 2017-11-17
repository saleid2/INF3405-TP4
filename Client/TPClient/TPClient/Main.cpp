#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>
#include <iostream>
#include <string>
#include <regex>


using namespace std;

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

/* Global const */
#define IPV4_LENGTH 16
#define PORT_MAX_LENGTH 5

/* Function prototyping */

void requestPortAndIP(string* ipAddress, string* &port);
bool validateIP(string* ip);
bool validatePort(string* port);


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

	string* ipAddress = new string;
	string* port = new string;

	requestPortAndIP(ipAddress, port);

	// Resolve the server address and port
	/*
	iResult = getaddrinfo(ipAddress, port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	*/
	// TO CONTINUE

	delete ipAddress;
	delete port;

	return 0;
}

void requestPortAndIP(string* ipAddress, string* &port) {

	cout << "Launching chatroom" << endl;

	//char tempIP[IPV4_LENGTH];

	do {
		cout << "Please enter the server IP address: ";
		cin >> *ipAddress;
	} while (!validateIP(ipAddress));
	
	do {
		cout << "Please pick a port number between 10000 and 10050: ";
		cin >> *port;
	} while (!validatePort(port));

	cout << "Port ok";

	while (1);

	// TO CONTINUE
	/*
	void saisirParametres(char*& adresseIP, char*& port) {

		cout << "Parametres du serveur a joindre" << endl;

		char adresseIPTemp[LONGUEUR_ADRESSE_IP];
		//demander l'adresse IP tant qu'elle n'est pas valide
		do {
			cout << endl << "Entrer l'adresse IP du poste du serveur a joindre: ";
			gets_s(adresseIPTemp);
		} while (!estFormatIP(adresseIPTemp));

		//enregistrer l'adresse IP validée
		for (size_t i = 0; i < sizeof(adresseIPTemp); i++) {
			adresseIP[i] = adresseIPTemp[i];
		}

		char portTemp[LONGUEUR_PORT];
		//demander le port tant qu'il n'est pas valide
		do {
			cout << endl << "Entrer le port d'ecoute (entre 6000 et 6050): ";
			gets_s(portTemp);
		} while (!estPortValide(portTemp));

		//enregistrer le port validé
		for (size_t i = 0; i < sizeof(portTemp); i++) {
			port[i] = portTemp[i];
		}

	}
	*/

}

bool validateIP(string* ip) {

	regex ipRegex("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");

	bool isValidIp = regex_search(*ip, ipRegex);

	if (!isValidIp) {
		cout << "The IP address is not valid. Please enter a valid IP adress." << endl;
	}

	return (isValidIp);
}

bool validatePort(string* port) {

	regex ipRegex("^100([0-4][0-9]|50)$");

	bool isValidIp = regex_search(*port, ipRegex);

	if (!isValidIp) {
		cout << "The port number is not valid. Please enter a valid port number between 10000 and 100050." << endl;
	}

	return (regex_search(*port, ipRegex));
}