/* ###################################### */
/* #    TSAM - Project 3: BotNet        # */
/* #                                    # */
/* #    Þórir Ármann Valdimarsson       # */
/* #    Smári Freyr Guðmundsson         # */
/* #    Snorri Arinbjarnar              # */
/* #                                    # */
/* ###################################### */

// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// System includes
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

// Data structure includes
#include <vector>
#include <map>
#include <string>
#include <string.h>

// Time includes
#include <time.h>

// Stream includes
#include <iostream>
#include <sstream>

// Thread includes
#include <pthread.h>

/* ### Constants ### */

// Buffer sizes
#define MINBUFFERSIZE 16
#define MEDBUFFERSIZE 64
#define LARGEBUFFERSIZE 256
#define XLARGEBUFFERSIZE 2048
#define XXLARGEBUFFERSIZE 8192

// Sockets and ports
#define CLIENTSOCKET 0
#define SERVERSOCKET 1
#define SERVERLISTSOCKET 2

#define PORTAMOUNT 3

#define MAXCLIENTSALLOWED 8

#define PORTLOWERBOUND 30000
#define PORTUPPERBOUND 60000

// Other constants.
#define STARTSYMBOL '\01'
#define ENDSYMBOL '\04'

/* ### Namespace ### */
using namespace std;

/* ### Data structures ### */

// There is one configuration for each listening socket. This struct
// contains the port number and address information which each socket
// is bound to.
struct serverConfiguration
{
    int portNumber;
    int serverSocketDescriptor;
    struct sockaddr_in serverSocketAddress;
};

// Each user which has made a connection is allocated an instance of
// this struct. It can be used to find out what socketFd belongs to
// a userName and vice versa. isReceiving tells the server whether a
// chatUser should be sent messages.
struct chatUser
{
    string userName;
    bool isReceiving;
    int socketFd;
};

// Each server which has made a connection is allocated an instance of
// this struct. It can be used to find out what socketFd belongs to
// an id and vice versa. timeSinceLastActivity tells us the most recent
// time a server sent a KEEPALIVE command.
struct directlyConnectedServer
{
    string id;
    int socketFd;
    in_addr_t ip;
    string ipString;
    uint16_t port;
    string portString;
    time_t timeSinceLastActivity;
};

/* ### Global variables ### */

// Vector that contains the users currently connected to the server.
// Each user contains a unique username and a unique socket file descriptor.
vector<chatUser> currentUsers;

// Vector which contains the directly connected servers.
vector<directlyConnectedServer> directlyConnectedServers;

// vector containing the hashes of out server.
vector<string> serverHashes;

// Configurations for our server.
serverConfiguration configurations[3];

// The server's id. Used to get bonus points.
// Can be viewed by the client and other servers.
string Id;

// The three ports this server listens to.
int serverPort, serverListPort, clientPort;

// This file descriptor set contains the socket file descriptors currently
// set on this server. It initially contains the three listening sockets but
// subsequent client/server socket descriptors will be added to it.
fd_set mainFileDescriptorSet;

/* ### Server/Client communication functions ### */
// Processes input from already connected clients. This is essentially the server's API. API commands are
// interpreted here and corresponding function calls are made.
void checkAPI(string input, int socketFileDescriptor);

// An api for the servers communicating with our server.
void checkServerAPI(vector<string> messageFromServer, int socketFileDescriptor);

// Is used in a few cases. Sends a message to <clientSocketDescriptor> whether an action failed or not.
void sendFeedback(bool success, int clientSocketDescriptor);

// This function gets called when the client requests info the server Id. It simply sends the current Id to him.
void sendIdToClient(int clientSocketDescriptor);

// This function takes the vector of current users, parses the contents into a single white-space separated list
// of usernames and sends to the client.
void sendServerListOrUserListToClientOrServer(int clientSocketDescriptor, bool servers, bool toClient);

// This function loops through all active users, excluding the sender, and sends them message.
void sendMessageToAllUsers(string message, int clientSocketDescriptor);

// This function finds user with socketFd == clientSocketDescriptor and sends him message.
void sendMessageToUser(string message, int clientSocketDescriptor, string receivingUser);

// This function removes the user from the main file descriptor set and closes his connection.
void disconnectUser(int socketFileDescriptor);

void sendHashStringToClient(int index, int socketFileDescriptor);
/* ### Server-side private functions ### */

// This function is only called once upon server initialization. It is used to dynamically allocate listening ports
// for the server. It scans the ports on the local machine for closed ports. When three consecutive closed ports
// are found, the global port variables are set and the function terminates. It also configures the addresses as well
// as binding them to the listening sockets. Finally, it makes the sockets non-blocking and sets their listening flags.
void initializeServer(struct serverConfiguration *configurations);

// Is used by checkAPI to split input strings from client thus: ABCDEFGH... A, B, CDEFGH...
// Is useful to interpret the API commands, to e.g. separate the actual message from the MSG <USER> command.
void splitString(vector<string> &inputCommands, string input);

// Split on a specific delimeter.
void delimeterStringSplitter(vector<string> &stringVec, string stringToSplit, char delimeter);

// This is used when making sure that we do not create > 1 users with the same user name.
// Also used to make sure that messages are not sent to non-existent users.
bool userExists(string user);

// Print error message and exit the program.
void printErrorAndQuit(string errorMsg);

// Encode message string about to be sent to other server.
string encodeMessageString(string strToEncode);
string getHashString(int index);


// Decode string from other servers.
vector<string> decodeMessageString(char *strToEncode);


vector<string> initHashVector(vector<string> hashVector);

// Restarts KeepAlive timer for server with socketFileDescriptor.
void restartTimer(int socketFileDescriptor);

// Connect our server to another server.
void connectServerToServer(string ip, string port);

// Create and add directly connected server.
void createAndAddDirectlyConnectedServer(int socket, in_addr_t ip, string ipString, uint16_t port, string ipPort);

// If servers have been inactive for some time, they are timed out and disconnected.
void disconnectInactiveServers();

// A thread that keeps conneections to directly connected servers alive.
void *keepAlive(void *ptr);

// Server start point.
int main(int argv, char *args[])
{
    // 2 port numbers must be entered as arguments.
    if (argv != 3) { printErrorAndQuit("Please pass two port numbers as argument"); }
    serverPort = atoi(args[1]);
    serverListPort = atoi(args[2]);
    clientPort = 1337;

    // Each socket has one configuration.

    // Open the sockets endpoint for incoming connections and make them non-blocking.
    configurations[CLIENTSOCKET].serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    configurations[SERVERSOCKET].serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    configurations[SERVERLISTSOCKET].serverSocketDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Make the sockets reusable.
    int q = 1;
    setsockopt(configurations[CLIENTSOCKET].serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &q, sizeof(q));
    setsockopt(configurations[SERVERSOCKET].serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &q, sizeof(q));
    setsockopt(configurations[SERVERLISTSOCKET].serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &q, sizeof(q));

    // Finish initialization.
    initializeServer(configurations);
    serverHashes = initHashVector(serverHashes);


    // Set the server Id.
    Id = "V_Group_26";

    // We will use file descriptor sets to maintain our incoming socket connections
    // The set main is our main file descriptor set. We start by zeroing it out.
    FD_ZERO(&mainFileDescriptorSet);

    // Add our listening sockets to the set.
    FD_SET(configurations[CLIENTSOCKET].serverSocketDescriptor, &mainFileDescriptorSet);
    FD_SET(configurations[SERVERSOCKET].serverSocketDescriptor, &mainFileDescriptorSet);
    FD_SET(configurations[SERVERLISTSOCKET].serverSocketDescriptor, &mainFileDescriptorSet);

    // This address variable is used to peek at incoming connection requests during
    // the knocking sequence. To access the IP address in order to use it as a key
    // for the port knocking map.
    struct sockaddr_in connectingClientAddress;
    socklen_t connectingClientAddressSize = sizeof connectingClientAddress;


    // Run thread for keepAlive.
    pthread_t keepAliveThread;
    int threadFail = pthread_create(&keepAliveThread, NULL, keepAlive, NULL);
    if(threadFail) {
        perror("THREAD failure");
        exit(1);
    }

    // Server loop. Loops continuously and processes connection requests.
    while(true) {
        // The select() function alters our main set. Thus we must
        // keep a copy of it for every iteration of the loop.
        fd_set mainFileDescriptorSetBackup = mainFileDescriptorSet;

        // We use select() to handle connections from multiple clients. If a new connection
        // passes the port knocking, it is accepted and the client's socket file descriptor
        // is added to the fd_set. It returns when one or more descriptor in the set is ready
        // to be read.
        if (select(FD_SETSIZE, &mainFileDescriptorSetBackup, NULL, NULL, NULL) < 0) {
            perror("SELECT error");
            exit(1);
        }

        // One ore more socket descriptors are ready to read. Next step is to loop through the
        // fd_set and check which ones they are.
        for (int i = 0; i < FD_SETSIZE; i++) {
            // Check if socket file descriptor i is set in mainFileDescriptorSetBackup.
            if (FD_ISSET(i, &mainFileDescriptorSetBackup)) {
                // If i is one of the listening sockets, that means
                // we have a new connection. One of our listening
                // sockets has received a connection request.
                if (i == configurations[CLIENTSOCKET].serverSocketDescriptor || i == configurations[SERVERSOCKET].serverSocketDescriptor || i == configurations[SERVERLISTSOCKET].serverSocketDescriptor) {
                    int newSocketDescriptor = accept(i, (struct sockaddr *)&connectingClientAddress, &connectingClientAddressSize);
                    string incomingIpAddress = (string)(inet_ntoa(connectingClientAddress.sin_addr));
                    char incomingPort[128];
                    sprintf(incomingPort, "%i", connectingClientAddress.sin_port);

                    // Here we determine what kind of connection is inbound.
                    int portNum;
                    for (int j = 0; j < PORTAMOUNT; j++) {
                        if (configurations[j].serverSocketDescriptor == i) {
                            if(j == CLIENTSOCKET) {
                                cout << "A client has connected" << endl;
                                FD_SET(newSocketDescriptor, &mainFileDescriptorSet);
                            }
                            else if (j == SERVERSOCKET) {
                                cout << "A server has connected" << endl;
                                createAndAddDirectlyConnectedServer(
                                    newSocketDescriptor,
                                    connectingClientAddress.sin_addr.s_addr,
                                    incomingIpAddress,
                                    connectingClientAddress.sin_port,
                                    incomingPort
                                );

                                // When ever a directly connected server tries to connect,
                                // we should send a command to get that servers id, to
                                // add to or global vector of direcly connected servers.
                                checkAPI("CMD,," + Id + ",ID", newSocketDescriptor);
                            }
                            else if (j == SERVERLISTSOCKET) {
                                cout << "UDP received" << endl;
                                char udpReceived[MEDBUFFERSIZE];
                                memset(udpReceived, 0, sizeof udpReceived);
                                ssize_t receivedUDPBytes = recvfrom(configurations[SERVERLISTSOCKET].serverSocketDescriptor, udpReceived, sizeof udpReceived, 0, NULL, NULL);
                                if(receivedUDPBytes < 0) { perror("UDPRecvFrom error"); }
                                if(udpReceived[0] == '\01') {
                                    checkServerAPI(decodeMessageString(udpReceived), -1337);
                                }
                            }
                            portNum = configurations[j].portNumber;
                        }
                    }
                }

                // If i is not one of the listening sockets, it must be
                // an outside connection from client already in the fd_set,
                // containing a message.
                else if (i != configurations[CLIENTSOCKET].serverSocketDescriptor || i != configurations[SERVERSOCKET].serverSocketDescriptor || i != configurations[SERVERLISTSOCKET].serverSocketDescriptor) {
                    // Receive message from client i.
                    char receiveBuffer[XXLARGEBUFFERSIZE];
                    memset(receiveBuffer, 0, sizeof receiveBuffer);
                    bool isClient = true;
                    int bytesReceived = recv(i, receiveBuffer, sizeof receiveBuffer, 0);

                    // Check if message is from server in set and check if that server has legal
                    // encoding, if not, nothing happens. Else, the message must be from a client.
                    for(size_t k = 0; k < directlyConnectedServers.size(); k++) {
                        if(directlyConnectedServers[k].socketFd == i) {
                            if(receiveBuffer[0] == '\01') {
                                checkServerAPI(decodeMessageString(receiveBuffer), i);
                            }
                            else { isClient = false; }
                            break;
                        }
                    }

                    if(isClient){
                        // The message will be interpreted and proccessed in checkAPI.
                        checkAPI(receiveBuffer, i);
                    }
                }
            }
        }
    }

    // Close connections before termination,
    // zero out filedescriptor set and join threads.
    for (size_t i = 0; i < currentUsers.size(); i++) { close(currentUsers[i].socketFd); }
    FD_ZERO(&mainFileDescriptorSet);
    pthread_join(keepAliveThread, NULL);

    return 0;
}

// Encode string to standards to send message from server.
string encodeMessageString(string strToEncode) {
    return STARTSYMBOL + strToEncode + ENDSYMBOL;
}

// Decode string from servers.
vector<string> decodeMessageString(char *strToEncode) {
    string str(strToEncode);
    vector<string> messageFromServer;
    delimeterStringSplitter(messageFromServer, str, ',');

    int lastIndexOfMsg = messageFromServer.size() - 1;

    // Cut the first encoding character off the string.
    messageFromServer[0] = &messageFromServer[0][1];
    // Cut the last encoding character off the string.
    messageFromServer[lastIndexOfMsg] = messageFromServer[lastIndexOfMsg].substr(0, messageFromServer[lastIndexOfMsg].length() - 1);

    return messageFromServer;
}

/* ### Server/Server communication functions ### */

// Sends KEEPALIVE commands to every connected server every 90 seconds.
void *keepAlive(void *ptr) {
    char keepAliveBuffer[MEDBUFFERSIZE];

    while(true) {
        usleep(90000000);
        if(directlyConnectedServers.size() > 0) {
            disconnectInactiveServers();
            for(size_t i = 0; i < directlyConnectedServers.size(); i++) {
                if(directlyConnectedServers[i].id != "") {
                    cout << "Sending KEEPALIVE to: " << directlyConnectedServers[i].id << endl;

                    // FORMAT: CMD,<TO_ID: EMPTY(ONEHOP)>,<FROM_ID(THIS_SERVER)>,KEEPALIVE
                    string strToEncode = "CMD,," + Id + ",KEEPALIVE";
                    string strToSend = encodeMessageString(strToEncode);
                    strcpy(keepAliveBuffer, strToSend.c_str());

                    if (send(directlyConnectedServers[i].socketFd, keepAliveBuffer, sizeof keepAliveBuffer, 0) < 0) {
                        perror("KEEPALIVE send_fail");
                    }

                    memset(keepAliveBuffer, 0, sizeof keepAliveBuffer);
                }
            }
        }
    }
}

// Restarts KeepAlive timer for server with socketFileDescriptor.
void restartTimer(int socketFileDescriptor) {
    for(size_t i = 0; i < directlyConnectedServers.size(); i++) {
        if(directlyConnectedServers[i].socketFd == socketFileDescriptor) {
            cout << "Restarting timer for: " << directlyConnectedServers[i].id << endl;
            time(&directlyConnectedServers[i].timeSinceLastActivity);
            break;
        }
    }
}

// An API for servers sending commands to another server.
void checkServerAPI(vector<string> messageFromServer, int socketFileDescriptor) {

    // TODO: Check if message being sent from another server are ment for us.
    // If not the message should be forwarded to another node. To decide where to
    // forward the message, we should look through are routing table to see where
    // we should send the message. If there is no path to the receiver of the message,
    // we should drop the message.

    if(messageFromServer[0] == "CMD") {

        // A connected server is issuing a command to get our ID.
        if(messageFromServer[1] == "" && messageFromServer[3] == "ID") {

            // We send a response back in this format:
            // RSP,<TOSERVERID>,<FROMSERVERID>,<command being responded>,<our server id>,<our ip address>,<our tcp port>
            string portStr;
            stringstream stringstream;
            stringstream << serverPort;
            portStr = stringstream.str();

            string strToSend = "RSP," + messageFromServer[2] + "," + Id + ",ID," + Id +"," + "130.208.243.61," + portStr + ";";
            strToSend = encodeMessageString(strToSend);

            char sendBuffer[strToSend.length() + 1];
            memset(sendBuffer, 0, sizeof sendBuffer);

            strcpy(sendBuffer, strToSend.c_str());

            if (send(socketFileDescriptor, sendBuffer, sizeof sendBuffer, 0) < 0) {
                perror("Sending Id to directly connected server failed.");
            }
        }
        if(messageFromServer[1] == "" && messageFromServer[3] == "KEEPALIVE") {
            restartTimer(socketFileDescriptor);
        }

        if(messageFromServer[3] == "LISTSERVERS" && messageFromServer[1] == Id && socketFileDescriptor == -1337) {
            // Thus is UDP so socketDescriptor does not come to use,
            // thus we send -1 as first parameter.
            cout << "UDP ListServers!" << endl;
            sendServerListOrUserListToClientOrServer(-1, true, false);
        }

        if(messageFromServer[3] == "FETCH") {
        	string fetchIndex = messageFromServer[4];

        	int index = stoi(fetchIndex);
        	string hashString = getHashString(index);

        	string strToSend = "RSP," + messageFromServer[2] + "," + Id +  ",FETCH," + messageFromServer[4] + "," + hashString + ";";
            strToSend = encodeMessageString(strToSend);

            char sendBuffer[strToSend.length() + 1];
            memset(sendBuffer, 0, sizeof sendBuffer);

            strcpy(sendBuffer, strToSend.c_str());

            if (send(socketFileDescriptor, sendBuffer, sizeof sendBuffer, 0) < 0) {
                perror("Sending hash to directly connected server failed.");
            }

            char receiveBuffer[XXLARGEBUFFERSIZE];
            memset(receiveBuffer, 0, sizeof receiveBuffer);

            int bytesReceived = recv(socketFileDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

            cout << "Fetched hash string: " << receiveBuffer << endl;

            //Senda til baka til checkAPI??
            if (send(socketFileDescriptor, receiveBuffer, sizeof receiveBuffer, 0) < 0) {
                perror("Sending hash to directly connected server failed.");
            }
        }

    }
    else if(messageFromServer[0] == "RSP") {

        // Check if RSP is a response from server containing the ID, IP and port.
        // The response from server is expected to be on this format:
        // RSP,<TO OUR SERVER ID>,<FROM SERVER ID>,<COMMAND>,<their id>,<their ip>,<their server tcp port>;
        if(messageFromServer.size() == 7 && messageFromServer[6][messageFromServer[6].length() - 1] == ';') {

            // Loop through all directly connected servers and find the server sending the response.
            // After finding the server, set the id. ip string and port string for that server.
            for(size_t i = 0; i < directlyConnectedServers.size(); i++) {
                if (directlyConnectedServers[i].socketFd == socketFileDescriptor) {
                    directlyConnectedServers[i].id = messageFromServer[4];
                    directlyConnectedServers[i].ipString = messageFromServer[5];
                    directlyConnectedServers[i].portString = messageFromServer[6];
                    cout << "====== THE SERVER WITH ID: " << messageFromServer[4] << " HAS NOW CONNECTED TO THE SERVER =====" << endl;
                    break;
                }
            }
        }
        if(messageFromServer[3] == "KEEPALIVE") {
            // TODO: implement
        }
        if(messageFromServer[3] == "FETCH"){

        	cout << "Hashstring: " << messageFromServer[5] << endl;
        	string strToSend = messageFromServer[5];
        	char sendBuffer[strToSend.length() + 1];
            memset(sendBuffer, 0, sizeof sendBuffer);

            strcpy(sendBuffer, strToSend.c_str());

            if (send(socketFileDescriptor, sendBuffer, sizeof sendBuffer, 0) < 0) {
                perror("Sending hash to directly connected server failed.");
            }
        }
    }
}

vector<string> initHashVector(vector<string> hashVector){
	vector<string> temp = {};
	temp.push_back("6864f389d9876436bc8778ff071d1b6c");
	temp.push_back("22811dd94d65037ef86535740b98dec8");
	temp.push_back("4e8f41fc6bb2a085340ba742f4bb655f");
	temp.push_back("ed735d55415bee976b771989be8f7005");
	temp.push_back("13b5bfe96f3e2fe411c9f66f4a582adf");

	return temp;
}

string getHashString(int index){
	return serverHashes[index];
}

void sendHashStringToClient(int index, int socketFileDescriptor){
	cout << "goes into sendHashStringToClient" << endl;
	string hashString = getHashString(index);
	//string hashString = "3e8f2856fcbe139da02f92894b714ca2";
	cout << "has gotten hash string from getHashString function" << endl;

	char sendBuffer[hashString.length() + 1];
    memset(sendBuffer, 0, sizeof sendBuffer);

    strcpy(sendBuffer, hashString.c_str());

	    if (send(socketFileDescriptor, sendBuffer, sizeof sendBuffer, 0) < 0) {
	        perror("Sending hash to client  failed.");
	    }

}

/* ### Server/Client communication functions ### */

// Processes input from already connected clients. This is essentially the server's API. API commands are
// interpreted here and corresponding function calls are made.
void checkAPI(string input, int socketFileDescriptor) {
    vector<string> inputCommands;
    splitString(inputCommands, input);

    vector<string> splitCommand;
    delimeterStringSplitter(splitCommand, input, ',');

    if (inputCommands[0] == "ID") { sendIdToClient(socketFileDescriptor); }
    else if (inputCommands[0] == "LEAVE") { disconnectUser(socketFileDescriptor); }
    else if (inputCommands[0] == "WHO") { sendServerListOrUserListToClientOrServer(socketFileDescriptor, false, true); }
    else if (inputCommands[0] == "MSG" && userExists(inputCommands[1])) { sendMessageToUser(inputCommands[2], socketFileDescriptor, inputCommands[1]); }
    else if (inputCommands[0] == "MSG" && inputCommands[1] == "ALL") { sendMessageToAllUsers(inputCommands[2], socketFileDescriptor); }
    else if (inputCommands[0] == "LISTSERVERS") { sendServerListOrUserListToClientOrServer(socketFileDescriptor, true, true); }
    else if (inputCommands[0] == "KEEPALIVE") { cout << "TODO: Implement KEEPALIVE" << endl; }
    else if (inputCommands[0] == "LISTROUTES") { cout << "TODO: Implement LISTROUTES" << endl; }
    else if (splitCommand[0] == "FETCH"){
    	cout << "sending to sendHashStringToClient function" << endl;
    	sendHashStringToClient(stoi(splitCommand[1]), socketFileDescriptor);
    }
    else if (splitCommand[0] == "CMD") {

        // We send a command in this format:
        //CMD,<TOSERVERID>,<FROMSERVERID>,<command>
        string strToSend = encodeMessageString(input);

        char sendBuffer[strToSend.length() + 1];
        memset(sendBuffer, 0, sizeof sendBuffer);

        strcpy(sendBuffer, strToSend.c_str());
        int fdToSend = -1;

        if(splitCommand[3] == "FETCH"){
        	for(size_t i = 0; i < directlyConnectedServers.size(); i++) {
                if(directlyConnectedServers[i].id == splitCommand[1]) {
                    fdToSend = directlyConnectedServers[i].socketFd;
                }
            }
        }
        if(splitCommand[1] == "" && splitCommand[3] == "ID") {
            fdToSend = socketFileDescriptor;
        }
        else if(splitCommand[1] != "" && splitCommand[3] == "ID") {
            // Loopa i gegnum serverID til ad finna rettan sockDescriptor.
            for(size_t i = 0; i < directlyConnectedServers.size(); i++) {
                if(directlyConnectedServers[i].id == splitCommand[1]) {
                    fdToSend = directlyConnectedServers[i].socketFd;
                }
            }

            if(fdToSend == -1) { perror("Id was not found for this server id."); }
        }
        if(fdToSend != -1) {
            if (send(fdToSend, sendBuffer, sizeof sendBuffer, 0) < 0) {
                perror("Sending CMD to directly connected server failed");
            }
        }
    }
    else if (inputCommands[0] == "CONNECT") {
        if (inputCommands[1] != "") {
            if (!userExists(inputCommands[1])) {
                chatUser newUser;
                newUser.userName = inputCommands[1];
                newUser.socketFd = socketFileDescriptor;
                newUser.isReceiving = false;
                currentUsers.push_back(newUser);
                sendFeedback(true, socketFileDescriptor);
            }
            else { sendFeedback(false, socketFileDescriptor); }
        }
        else { sendFeedback(false, socketFileDescriptor); }
    }
    else if (inputCommands[0] == "RECV") {
        for (size_t i = 0; i < currentUsers.size(); i++) {
            if (currentUsers[i].socketFd == socketFileDescriptor) {
                currentUsers[i].isReceiving = true;
                break;
            }
        }
    }
    else if (splitCommand[0] == "SCONNECT") {
        connectServerToServer(splitCommand[1], splitCommand[2]);
    }
}

// Called when our server is connecting to another server of the botnet.
void connectServerToServer(string ip, string port) {
    char buf[ip.size()];
    memset(buf, 0, sizeof buf);
    strcpy(buf, ip.c_str());

    in_addr_t serverIpAddress = inet_addr(buf);
    uint16_t serverPort = htons(atoi(port.c_str()));

    int connectServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in connectServerAddress;

    connectServerAddress.sin_family = AF_INET;
    connectServerAddress.sin_addr.s_addr = serverIpAddress;
    connectServerAddress.sin_port = serverPort;

    if(connect(connectServerSocket, (struct sockaddr *)&connectServerAddress, sizeof connectServerAddress) == 0) {
        createAndAddDirectlyConnectedServer(connectServerSocket, serverIpAddress, ip, serverPort, port);
        cout << "Connected to server: " << ip << ":" << port << endl;

        // Fetch id for that server and add to struct.
        checkAPI("CMD,," + Id + ",ID", connectServerSocket);

        return;
    }
    perror("SERVER_CONNECT failure");
}

// Add the directly connected server to the global vector containing the 1 hop away servers.
void createAndAddDirectlyConnectedServer(int socket, in_addr_t ip, string ipString, uint16_t port, string portString) {
    FD_SET(socket, &mainFileDescriptorSet);
    directlyConnectedServer server;
    server.id = "";
    server.socketFd = socket;
    server.ip = ip;
    server.ipString = ipString;
    server.port = port;
    server.portString = portString;
    time(&server.timeSinceLastActivity);
    directlyConnectedServers.push_back(server);
}

void disconnectInactiveServers() {
    time_t now;
    time(&now);
    bool removeIndices[directlyConnectedServers.size()];

    // We don't want to alter the vector while looping through it. Thus we keep the indices
    // of servers that should be removed in the bool array.
    for(size_t i = 0; i < directlyConnectedServers.size(); i++) {
        removeIndices[i] = false;
        double elapsedTimeInSec = difftime(now, directlyConnectedServers[i].timeSinceLastActivity);
        if(elapsedTimeInSec > 90) {
            removeIndices[i] = true;
        }
    }

    vector<directlyConnectedServer> newServerList;
    for(size_t j = 0; j < directlyConnectedServers.size(); j++) {
        if(removeIndices[j]) {
            cout << directlyConnectedServers[j].id << " timed out!" << endl;
            FD_CLR(directlyConnectedServers[j].socketFd, &mainFileDescriptorSet);
            close(directlyConnectedServers[j].socketFd);
            continue;
        }
        newServerList.push_back(directlyConnectedServers[j]);
    }
    directlyConnectedServers = newServerList;
}

// Is used in a few cases. Sends a message to <clientSocketDescriptor> whether an action failed or not.
void sendFeedback(bool success, int clientSocketDescriptor) {
    if (success) {
        char sendBackBuffer[MINBUFFERSIZE] = "SUCCESS";
        send(clientSocketDescriptor, sendBackBuffer, sizeof sendBackBuffer, 0);
    }
    else {
        char sendBackBuffer[MINBUFFERSIZE] = "FAIL";
        send(clientSocketDescriptor, sendBackBuffer, sizeof sendBackBuffer, 0);
    }
}

// This function gets called when the client requests info the server Id. It simply sends the current Id to him.
void sendIdToClient(int clientSocketDescriptor) {
    char sendIdBuffer[XLARGEBUFFERSIZE];
    strcpy(sendIdBuffer, Id.c_str());
    if (send(clientSocketDescriptor, sendIdBuffer, sizeof sendIdBuffer, 0) < 0) { perror("ID_SEND failure"); }
}

// This function takes the vector of current users or servers, parses the contents into a
// single white-space separated list and sends to the client.
void sendServerListOrUserListToClientOrServer(int clientSocketDescriptor, bool servers, bool toClient) {
    string listStringified = "";

    if(servers) {
        for (size_t i = 0; i < directlyConnectedServers.size(); i++) {
            // Format the server list into a sendable string.
            listStringified.append(directlyConnectedServers[i].id + "," +
                                   directlyConnectedServers[i].ipString + "," +
                                   directlyConnectedServers[i].portString + ";");
        }
    }
    else {
        for (size_t i = 0; i < currentUsers.size(); i++) {
            // Format the user list into a sendable string.
            listStringified.append(currentUsers[i].userName);
            if (i != (currentUsers.size() - 1)) { listStringified.append(" "); }
        }
    }

    // Send it.
    char sendUsersBuffer[listStringified.length() + 1];
    memset(sendUsersBuffer, 0, sizeof sendUsersBuffer);
    strcpy(sendUsersBuffer, listStringified.c_str());

    if(toClient) {
        if (send(clientSocketDescriptor, sendUsersBuffer, sizeof sendUsersBuffer, 0) < 0) { perror("USERLIST_SEND failure"); }
    }
    else {
        cout << "Sending UDP datagram of servers" << endl;
        if(sendto(configurations[SERVERLISTSOCKET].serverSocketDescriptor, sendUsersBuffer, sizeof sendUsersBuffer, 0, NULL, 0) < 0) {
            perror("SEND_LISTSERVERS error");
        }
    }
}

// This function loops through all active users, excluding the sender, and sends them message.
void sendMessageToAllUsers(string message, int clientSocketDescriptor) {
    // Find user sending the message.
    string sendingUser = "";
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].socketFd == clientSocketDescriptor) { sendingUser = currentUsers[i].userName; }
    }

    // Assemble the message.
    message = sendingUser + ": " + message;

    char sendMessageBuffer[message.length() + 1];
    strcpy(sendMessageBuffer, message.c_str());

    // Send loop.
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].isReceiving && currentUsers[i].socketFd != clientSocketDescriptor) {
            if (send(currentUsers[i].socketFd, sendMessageBuffer, sizeof sendMessageBuffer, 0) < 0) { perror("server failure: failed to send message"); }
        }
    }
}

// This function finds user with socketFd == clientSocketDescriptor and sends him message.
void sendMessageToUser(string message, int clientSocketDescriptor, string receivingUser) {
    // Find user sending the message and the fd for the receiving user.
    string sendingUser = "";
    int receivingClientSocketDescriptor;
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].socketFd == clientSocketDescriptor) { sendingUser = currentUsers[i].userName; }
        if (currentUsers[i].userName == receivingUser) { receivingClientSocketDescriptor = currentUsers[i].socketFd; }
    }

    // Assemble the message.
    message = "<PRIVATE> " + sendingUser + ": " + message;

    char sendMessageBuffer[message.length() + 1];
    strcpy(sendMessageBuffer, message.c_str());

    // Send the message.
    if (send(receivingClientSocketDescriptor, sendMessageBuffer, sizeof sendMessageBuffer, 0) < 0) { perror("server failure: failed to send message"); }
}

// This function removes the user from the main file descriptor set and closes his connection.
void disconnectUser(int socketFileDescriptor) {
    // Remove the user from fd_set.
    FD_CLR(socketFileDescriptor, &mainFileDescriptorSet);
    vector<chatUser> newUserList;

    // Update the user list.
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].socketFd != socketFileDescriptor) { newUserList.push_back(currentUsers[i]); }
    }
    currentUsers = newUserList;

    // Close connection.
    close(socketFileDescriptor);
}

/* ### Server-side private functions ### */

// This function is only called once upon server initialization. It is used to dynamically allocate listening ports
// for the server. It scans the ports on the local machine for closed ports. When three consecutive closed ports
// are found, the global port variables are set and the function terminates. It also configures the addresses as well
// as binding them to the listening sockets. Finally, it makes the sockets non-blocking and sets their listening flags.
void initializeServer(struct serverConfiguration *configurations) {

    // Configure the addresses.
    configurations[CLIENTSOCKET].serverSocketAddress.sin_family = AF_INET;
    configurations[CLIENTSOCKET].serverSocketAddress.sin_addr.s_addr = INADDR_ANY;

    configurations[SERVERSOCKET].serverSocketAddress.sin_family = AF_INET;
    configurations[SERVERSOCKET].serverSocketAddress.sin_addr.s_addr = INADDR_ANY;

    configurations[SERVERLISTSOCKET].serverSocketAddress.sin_family = AF_INET;
    configurations[SERVERLISTSOCKET].serverSocketAddress.sin_addr.s_addr = INADDR_ANY;

    configurations[CLIENTSOCKET].serverSocketAddress.sin_port = htons(clientPort);
    configurations[SERVERSOCKET].serverSocketAddress.sin_port = htons(serverPort);
    configurations[SERVERLISTSOCKET].serverSocketAddress.sin_port = htons(serverListPort);

    // Make the sockets non-blocking.
    int q = 1;
    ioctl(configurations[CLIENTSOCKET].serverSocketDescriptor, FIONBIO, (char *)&q);
    ioctl(configurations[SERVERSOCKET].serverSocketDescriptor, FIONBIO, (char *)&q);
    ioctl(configurations[SERVERLISTSOCKET].serverSocketDescriptor, FIONBIO, (char *)&q);

    // Assign name to sockets by binding the addresses to them.
    bind(configurations[CLIENTSOCKET].serverSocketDescriptor, (struct sockaddr *)&configurations[CLIENTSOCKET].serverSocketAddress, sizeof configurations[CLIENTSOCKET].serverSocketAddress);
    bind(configurations[SERVERSOCKET].serverSocketDescriptor, (struct sockaddr *)&configurations[SERVERSOCKET].serverSocketAddress, sizeof configurations[SERVERSOCKET].serverSocketAddress);
    bind(configurations[SERVERLISTSOCKET].serverSocketDescriptor, (struct sockaddr *)&configurations[SERVERLISTSOCKET].serverSocketAddress, sizeof configurations[SERVERLISTSOCKET].serverSocketAddress);

    // Mark the sockets as open for connections.
    listen(configurations[CLIENTSOCKET].serverSocketDescriptor, 5);
    listen(configurations[SERVERSOCKET].serverSocketDescriptor, 5);
    listen(configurations[SERVERLISTSOCKET].serverSocketDescriptor, 1);
}

// Is used by checkAPI to split input strings from client thus: ABCDEFGH... A, B, CDEFGH...
// Is useful to interpret the API commands, to e.g. separate the actual message from the MSG <USER> command.
void splitString(vector<string> &inputCommands, string input) {
    string tmp = "";
    int counter = 0;
    for (int i = 0; i < input.length(); i++) {
        if (input[i] == ' ' && counter < 2) {
            inputCommands.push_back(tmp);
            tmp = "";
            counter++;
        }
        else { tmp += input[i]; }
    }
    inputCommands.push_back(tmp);
}

void delimeterStringSplitter(vector<string> &stringVec, string stringToSplit, char delimeter) {
    string tmp = "";
    for (int i = 0; i < stringToSplit.length(); i++) {
        if (stringToSplit[i] == delimeter) {
            stringVec.push_back(tmp);
            tmp = "";
        }
        else if (i == (stringToSplit.length() - 1)) {
            tmp += stringToSplit[i];
            stringVec.push_back(tmp);
            break;
        }
        else { tmp += stringToSplit[i]; }
    }
}


// This is used when making sure that we do not create > 1 users with the same user name.
// Also used to make sure that messages are not sent to non-existent users.
bool userExists(string user) {
    for (int i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].userName == user) { return true; }
    }
    return false;
}


// Print error message and exit the program.
void printErrorAndQuit(string errorMsg) {
    cout << errorMsg << endl;
    cout << "Shutting down..." << endl;
    exit(1);
}
