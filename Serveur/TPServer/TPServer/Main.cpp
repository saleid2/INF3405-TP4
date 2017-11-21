#include <winsock2.h>	//Most of the Winsock functions, structures, and definitions
#include <ws2tcpip.h>	//Definitions introduced in the WinSock 2 Protocol-Specific Annex document for TCP/IP
#include <stdio.h>		//Input and outputs, printf
#include <regex>
//#include <minwinbase.h> // TODO: Check if this include is actually necessary
#include <sstream>		//String stream to convert int to string
#include <thread>
#include <fstream>
#include <list>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib") //Link with the Ws2_32.lib library file

#define MIN_PORT 5000
#define MAX_PORT 5050
#define IP_ADDR_MAX_LENGTH 16
#define PORT_LENGTH 4
#define MAX_MSG_LENGTH 200
#define MAX_HEADER_LENGTH 6
#define MAX_STORED_MSG 15
#define UN_PW_SEPARATOR "%__%"
#define USERS_FILE "users.txt"
#define USER_STATUS_OK 1
#define USER_STATUS_NOT_EXIST 0
#define USER_STATUS_WRONG_PW -1
#define MSG_TYPE_MSG "$msg:"
#define MSG_TYPE_LOGIN "$log:"
#define MSG_TYPE_LOGOUT "$quit:"
#define MSG_TYPE_LISTEN "$ok:"
#define LOGIN_STATUS_OK "$login_ok:"
#define LOGIN_STATUS_FAIL "$login_failed:"
#define LOG_MSG_FILE "log.txt"

// Mutex for modifying vector of sockets
HANDLE soMutex;

// Mutex for read/write to file for username/pw
HANDLE unMutex;

// Mutex for read/write of messages
HANDLE mgMutex;

// Mutex for message counter
HANDLE mcMutex;

// Counter that keeps track of new messages so that we can trigger write-to-disk every 15 messages
int mgCounter = 0;

std::list<std::string> savedMessages;

struct ChildThreadParams
{
	SOCKET sd_;
	std::vector<SOCKET>* openedSockets_;
	sockaddr_in sinRemote_;
};

extern DWORD WINAPI EchoHandler(void* ctp);
static bool SaveHistory();
static void LoadHistory();
static void SendMessageHistory(SOCKET sd);
static std::string FormatMessageBeforeSend(const std::string& un, const std::string& ip, const std::string& msg);
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

//Checks if IP address is valid
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
			ctp.openedSockets_ = &openSockets;
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

	SaveHistory();

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
	mgMutex = CreateMutex(NULL, FALSE, NULL);
	mcMutex = CreateMutex(NULL, FALSE, NULL);

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

	LoadHistory();

	openSocket(ipAddress, portNumber, listenSocket);

	WSACleanup();
	printf("Server closed successfully");

	system("pause");

	return 0;
}

static bool SaveHistory()
{
	bool success = true;

	WaitForSingleObject(mgMutex, INFINITE);
	WaitForSingleObject(mcMutex, INFINITE);

	std::ofstream out;
	out.open(LOG_MSG_FILE, std::ios::app); // Open and append at the end of file

	if(out)
	{
		auto it = savedMessages.begin();
		auto end = savedMessages.end();

		for (it; it != end; ++it)
		{
			out << *it << std::endl;
		}

		out.close();
		mgCounter = 0;
	}
	else
	{
		success = false;
	}

	ReleaseMutex(mcMutex);
	ReleaseMutex(mgMutex);
	return success;
}

static void LoadHistory()
{
	WaitForSingleObject(mgMutex, INFINITE);

	std::ifstream in;
	in.open(LOG_MSG_FILE);
	if(in)
	{
		savedMessages.clear();
		std::string msg;
		while(std::getline(in, msg))
		{
			if (savedMessages.size() >= MAX_STORED_MSG) savedMessages.pop_front();
			savedMessages.push_back(msg);
		}
		in.close();
	}

	ReleaseMutex(mgMutex);
}

DWORD WINAPI EchoHandler(void* ctp_)
{
	ChildThreadParams* ctp = (ChildThreadParams*)ctp_;
	SOCKET sd = ctp->sd_;

	bool disconnectRequested = false;
	bool errorOccured = false;

	char readBuffer[MAX_MSG_LENGTH + MAX_HEADER_LENGTH];
	std::string username = "";
	int readBytes;

	
	char ipAddr[INET_ADDRSTRLEN];
	u_short userPort;
	std::string userIP;

	InetNtop(AF_INET, &(ctp->sinRemote_.sin_addr), ipAddr, INET_ADDRSTRLEN);
	userPort = ntohs(ctp->sinRemote_.sin_port);


	std::stringstream ss;
	ss << ipAddr;
	ss << ":";
	ss << userPort;
	userIP = ss.str();

	//Read Data from client
	do
	{
		readBytes = 0;
		readBytes = recv(sd, readBuffer, MAX_MSG_LENGTH + MAX_HEADER_LENGTH, 0);

		if (readBytes != SOCKET_ERROR)
		{
			std::string msgData = readBuffer;
			msgData = msgData.substr(0, readBytes);
			std::string msgType = msgData.substr(0, msgData.find(':') + 1);
			
			msgData.erase(0, msgData.find(msgType) + msgType.length());

			if(msgType == MSG_TYPE_LOGIN)
			{
				
				std::string tempUN = msgData.substr(0, msgData.find(UN_PW_SEPARATOR));
				std::string tempPW = msgData.erase(0, msgData.find(UN_PW_SEPARATOR) + ((std::string)UN_PW_SEPARATOR).length());
				
				int loginStatus = LoginUser(tempUN, tempPW);
				if(loginStatus == USER_STATUS_OK)
				{
					username = tempUN;
					send(sd, (char*)LOGIN_STATUS_OK, 4*sizeof((char*)LOGIN_STATUS_OK), 0);
				}
				else if(loginStatus == USER_STATUS_WRONG_PW)
				{
					send(sd, (char*)LOGIN_STATUS_FAIL, 4*sizeof((char*)LOGIN_STATUS_FAIL), 0);
				}
				else if(loginStatus == USER_STATUS_NOT_EXIST)
				{
					bool userCreated = CreateUser(tempUN, tempPW);
					if (userCreated)
					{
						username = tempUN;
						send(sd, (char*)LOGIN_STATUS_OK, 4*sizeof((char*)LOGIN_STATUS_OK), 0);
					}
					else
					{
						send(sd, (char*)LOGIN_STATUS_FAIL, 4*sizeof((char*)LOGIN_STATUS_FAIL), 0);
					}
				}
			}
			else if (msgType == MSG_TYPE_LISTEN)
			{
				SendMessageHistory(sd);
			}
			else if (msgType == MSG_TYPE_MSG)
			{
				std::string formattedMsg = FormatMessageBeforeSend(username, userIP, msgData);

				WaitForSingleObject(mgMutex, INFINITE);
				WaitForSingleObject(mcMutex, INFINITE);

				// If 15 messages have been received since last save
				if(mgCounter == MAX_STORED_MSG)
				{
					//Release mutex before calling SaveHistory so that function can lock it
					ReleaseMutex(mcMutex);
					SaveHistory();
				}
				else
				{
					ReleaseMutex(mcMutex);
				}

				if(savedMessages.size() >= MAX_STORED_MSG) savedMessages.pop_front();
				savedMessages.push_back(formattedMsg);
				mgCounter++;

				std::string output = "Received message: " + formattedMsg + "\n";
				printf(output.c_str());

				WaitForSingleObject(soMutex, INFINITE);

				auto it = ctp->openedSockets_->begin();
				auto end = ctp->openedSockets_->end();
				for(it; it != end; ++it)
				{
					formattedMsg += "\n";
					send(*it, formattedMsg.c_str(), formattedMsg.length(), 0);
				}

				ReleaseMutex(soMutex);

				ReleaseMutex(mgMutex);
			}
			else if (msgType == MSG_TYPE_LOGOUT)
			{
				disconnectRequested = true;
			}
		}
		else
		{
			if(WSAGetLastError() != WSAECONNRESET) printf("%S\n", WSAGetLastErrorMessage());
			errorOccured = true;
		}
	}
	while (!disconnectRequested && !errorOccured);


	closesocket(sd);

	WaitForSingleObject(soMutex, INFINITE);

	// Remove closed socket from socket vector
	auto it = std::find(ctp->openedSockets_->begin(), ctp->openedSockets_->end(), sd);
	if(it != ctp->openedSockets_->end())
	{
		std::swap(*it, ctp->openedSockets_->back()); // Swap item with last one
		ctp->openedSockets_->pop_back(); // Remove last item from vector

		// Using swap + pop_back avoids the reshuffling that erase does
	}

	ReleaseMutex(soMutex);

	return 0;
}

static std::string FormatMessageBeforeSend(const std::string& un, const std::string& ip, const std::string& msg)
{
	time_t rawTime;
	struct tm timeInfo;
	time(&rawTime);
	localtime_s(&timeInfo, &rawTime);

	std::stringstream timeSS;

	timeSS << std::to_string(timeInfo.tm_year + 1900) + "-";
	timeSS << std::to_string(timeInfo.tm_mon + 1) + "-";
	timeSS << std::to_string(timeInfo.tm_mday) + "@";
	timeSS << std::to_string(timeInfo.tm_hour) + ":";
	timeSS << std::to_string(timeInfo.tm_min) + ":";
	timeSS << std::to_string(timeInfo.tm_sec);

	std::stringstream ss;
	ss << "[" + un + " - " + ip + " - " + timeSS.str() + "]: ";
	ss << msg;

	return ss.str();
}

static void SendMessageHistory(SOCKET sd)
{
	WaitForSingleObject(mgMutex, INFINITE);
	auto it = savedMessages.begin();
	auto end = savedMessages.end();
	for (it; it != end; ++it)
	{
		std::string msg = *it + "\n";
		send(sd, msg.c_str(), msg.length(), 0);
	}
	ReleaseMutex(mgMutex);
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
		printf("Cannot open %S\n", USERS_FILE);
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
	out.open(USERS_FILE, std::ios::app);
	bool success = false;
	WaitForSingleObject(unMutex, INFINITE);
	if (!out)
	{
		//Error occured!
		printf("Cannot open %S\n", USERS_FILE);
	}
	else
	{
		out << un + UN_PW_SEPARATOR + pw << std::endl;
		out.close();
		success = true;
	}
	ReleaseMutex(unMutex);

	return success;
}
