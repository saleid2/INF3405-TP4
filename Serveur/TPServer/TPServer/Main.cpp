#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>		//Input and outputs, printf
#include <regex>
//#include <minwinbase.h> // TODO: Check if this include is actually necessary
#include <sstream>		//String stream to convert int to string
#include <thread>
#include <forward_list>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

#define MIN_PORT 5000
#define MAX_PORT 5050
#define IP_ADDR_MAX_LENGTH 16
#define PORT_LENGTH 4
#define MAX_MSG_LENGTH 200
#define MAX_STORED_MSG 15

// TODO: Determine max username length to use on server & client
#define MAX_USERNAME_LENGTH 100
#define UN_PW_SEPARATOR "%__%"
#define USERS_FILE "users.txt"
#define USER_STATUS_OK 1;
#define USER_STATUS_NOT_EXIST 0;
#define USER_STATUS_WRONG_PW -1;

// MESSAGE IDENTIFIERS
#define MSG_TYPE_MSG "$msg:"
#define MSG_TYPE_LOGIN ="$log:"
#define MSG_TYPE_LOGOUT = "$quit:"

// Mutex pour section critiques de modifications du vecteur contenant tous les sockets ouverts
HANDLE soMutex;

// Mutex pour la lecture et ecriture du fichier de username et pw
HANDLE unMutex;

// Mutex pour la lecture et ecriture de messages
HANDLE mgMutex;

std::forward_list<std::string> savedMessages;

struct ChildThreadParams
{
	SOCKET sd_;
	std::vector<SOCKET> openedSockets_;
	sockaddr_in sinRemote_;
};

extern DWORD WINAPI EchoHandler(void* ctp);
static std::string GeneratePasswordforUsername(const std::string& un);
static int LoginUser(const std::string& un, const std::string& pw);
static bool CreateUser(const std::string& un, const std::string& pw);

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
	std::vector<HANDLE> openThreads;

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
			WaitForSingleObject(soMutex, INFINITE);

			openSockets.push_back(sd);

			ReleaseMutex(soMutex);

			ChildThreadParams ctp;
			ctp.sd_ = sd;
			ctp.openedSockets_ = openSockets;
			ctp.sinRemote_ = sinRemote;

			DWORD nThreadID;

			openThreads.push_back(CreateThread(nullptr, 0, EchoHandler, &ctp, 0, &nThreadID));
		}
		else
		{
			printf("%S\n", WSAGetLastErrorMessage());
		}
	}

	closesocket(sock);

	for(auto thread: openThreads)
	{
		TerminateThread(thread, 0);
	}

	for(auto sockets: openSockets)
	{
		closesocket(sockets);
	}

	return 0;
}

int main()
{

	soMutex = CreateMutex(NULL, FALSE, NULL);
	unMutex = CreateMutex(NULL, FALSE, NULL);

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

	WSACleanup();
	printf("Server closed successfully");
	
	system("pause");

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

	WaitForSingleObject(soMutex, INFINITE);

	// Remove closed socket from socket vector
	auto it = std::find(ctp->openedSockets_.begin(), ctp->openedSockets_.end(), sd);
	if(it != ctp->openedSockets_.end())
	{
		std::swap(*it, ctp->openedSockets_.back()); // Swap item with last one
		ctp->openedSockets_.pop_back(); // Remove last item from vector

		// Using swap + pop_back avoids the reshuffling that erase does
	}

	ReleaseMutex(soMutex);

	return 0;
}

static std::string GeneratePasswordforUsername(const std::string& un)
{
	std::string pw = un;
	std::random_shuffle(pw.begin(), pw.end());

	return pw;
}

static int LoginUser(const std::string& un, const std::string& pw)
{
	std::ifstream in;
	std::string strBuffer;
	int status = USER_STATUS_NOT_EXIST;

	WaitForSingleObject(unMutex, INFINITE);

	in.open(USERS_FILE);
	if (!in)
	{
		//Error occured!
		printf("Cannot open %s", USERS_FILE);
	}
	else
	{
		// Loop until end of file
		while (in >> strBuffer)
		{
			// Extract username from line
			std::string strUser = strBuffer.substr(0, strBuffer.find(UN_PW_SEPARATOR));
			strBuffer.erase(0, strBuffer.find(UN_PW_SEPARATOR) + ((std::string)UN_PW_SEPARATOR).length());

			if (strUser == un)
			{
				// Extract password from line
				std::string strPW = strBuffer;

				if(strPW == pw)
				{
					status = USER_STATUS_OK;
				}
				else
				{
					status = USER_STATUS_WRONG_PW;
				}
				// We can stop looping since we found the user
				break;
			}
		}
	}

	if (in.is_open()) in.close();

	ReleaseMutex(unMutex);

	return status;
}

static bool CreateUser(const std::string& un, const std::string& pw)
{
	std::ofstream out;
	WaitForSingleObject(unMutex, INFINITE);
	if (!out)
	{
		//Error occured!
		printf("Cannot open %s", USERS_FILE);
	}
	else
	{
		out.open(USERS_FILE, std::ios::app);
		out << un + UN_PW_SEPARATOR + pw << std::endl;
		out.close();
	}
	ReleaseMutex(unMutex);
}
