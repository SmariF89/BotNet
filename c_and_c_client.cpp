/* ###################################### */
/* #    TSAM - Project 2: Chatserver    # */
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
#include <netinet/in.h>

// Data structure includes
#include <vector>
#include <string>

// Other includes
#include <bits/stdc++.h>
#include <iostream>

/* ### Constants ### */

// Buffer sizes
#define MINBUFFERSIZE 16
#define MEDBUFFERSIZE 64
#define LARGEBUFFERSIZE 256
#define XLARGEBUFFERSIZE 2048
#define XXLARGEBUFFERSIZE 8192

// Other sizes
#define NUMBEROFPORTSTOKNOCK 3
#define ONESEC 1000000

// Other constants
#define LISTENINGSERVERPORT 1337

/* ### Namespace ### */
using namespace std;

/* ### Global variables ### */
int socketDescriptor;
string user;

/* ### Split functions ### */

// Split string on commas.
void commaStringSplitter(vector<string> &stringVec, string stringToSplit);

/* ### Print functions ### */

// Print first promt to user.
void printBeginPromptUserMessage();
// Print promt that the user is logged in.
void printLoggedInMessage();
// Print all commands available for user to enter.
void printCommands();
// Print error message and exit the program.
void printErrorAndQuit(string errorMsg);
// Print the standard interface line.
void printLine();

/* ### Other functions ### */

// Use API call CONNECT to validate the user name.
bool userNameIsValid(string input);
// Use API call ID to get and print the server ID.
void getAndPrintServerId();
// Use API call WHO get and print all users currently connected to the server.
void getAndPrintServerUsers();
// Use API calls MSG ALL or MSG to send a message to a specific person or all persons.
void sendMessage(string message, bool sendPrivate);
// Use API calls RECV to enter a receiving mode.
// Enter receive mode to receive messages from other connected users.
// The user must enter a time in seconds that he wiches to be in receive mode.
// After the receive mode, the user is free to make other commands.
void receiveMode();

// Use API calls LEAVE to leave the server and exit the program in a safe way.
bool leaveServerAndQuit();

bool connectToServer();

// Run the chatroom by calling functions that use the chat server API.
void runChatRoom(string input);


// Client start point.
int main(int argv, char *args[])
{
    // 3 port numbers must be entered as arguments.

    if(!connectToServer()) { printErrorAndQuit("Failed to connect to chat server."); }

    // Receive username from user. Loop will continue to run until a valid username is entered.
    string input;
    while (true) {
         printBeginPromptUserMessage();
         getline(cin, input);
         if (userNameIsValid(input)) { break; }
         else { cout << "Username is not valid, try again." << endl; }
    }

    // Chat room is runned with an inf loop that takes directs the user to various
    // functions that describe the user interface.
    runChatRoom(input);

    close(socketDescriptor);

    return 0;
}

/* ### Split functions ### */
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

/* ### Print functions ### */

// Print first promt to user.
void printBeginPromptUserMessage() {
    printLine();
    cout << "                                      WELCOME TO CHATRAT" << endl;
    printLine();
    cout << "Please enter a username to enter the chat." << endl;
}

// Print promt that the user is logged in.
void printLoggedInMessage() {
    printLine();
    cout << "                                 You are logged into the chat!" << endl;
}

// Print all commands available for user to enter.
void printCommands() {
    printLine();
    cout << "                                        List of commands" << endl;
    printLine();
    cout << " Input any of the following commands." << endl << endl;
    cout << " SND,<MESSAGE>                              (send message to all users on the chat)" << endl;
    cout << " SNDPR,<USERNAME>,<MESSAGE>                 (send a message to a specific user on the chat)" << endl;
    cout << " RECV,<TIME IN SEC>                         (Receive messages from users for a given time)" << endl;
    cout << " LST                                        (list all users on the chat)" << endl;
    cout << " SID                                        (see the id of server)" << endl;
    cout << " ESC                                        (Leave and disconnect from chat)" << endl << endl;
    cout << " MAN/HELP                                   (see list of commands available)" << endl;
    cout << " LISTSERVERS                                (list servers known from the connected server)" << endl;
    cout << " LISTROUTES                                 (List all servers in the network" << endl;
    cout << "                                             and the shortest paths to them)" << endl;
    cout << " CMD,<TOSERVERID>,<FROMSERVERID>,<command>  (enter command to a server from a server)" << endl;
    cout << " SCONNECT,<IP>,<PORT>                       (connect your server to server with <IP> and <PORT>)" << endl;
    cout << " FETCH,<no.>                                (Fetch hash value no. from a server)" << endl;
    printLine();
}

void printLine() {
    cout << "==================================================================================================" << endl;
}

// Print error message and exit the program.
void printErrorAndQuit(string errorMsg) {
    cout << errorMsg << endl;
    cout << "Shutting down..." << endl;
    exit(1);
}

/* ### Other functions ### */

// Use API call CONNECT to validate the user name.
bool userNameIsValid(string input) {
    // Create the correct string to send to server.
    input = "CONNECT " + input;
    char sendBuffer[input.length() + 1];
    char receiveBuffer[MINBUFFERSIZE];

    // Zero out("clean") buffers.
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);

    strcpy(sendBuffer, input.c_str());
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server if username is valid.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    if ((string)receiveBuffer == "SUCCESS") { return true; }
    return false;
}

// Use API call ID to get and print the server ID.
void getAndPrintServerId() {
    // Create the correct string to send to server.
    char sendBuffer[MINBUFFERSIZE] = "ID";
    char receiveBuffer[XLARGEBUFFERSIZE];
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server containing the server id and print out.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    printLine();
    fprintf(stderr, "SERVER ID:\n%s\n", receiveBuffer);
    printLine();

    // Zero out("clean") buffers.
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);
}

// Use API call WHO get and print all users currently connected to the server.
void getAndPrintServerUsers() {
    // Create the correct string to send to server.
    char sendBuffer[MINBUFFERSIZE] = "WHO";
    char receiveBuffer[XXLARGEBUFFERSIZE];
    vector<string> receivedUserNamesVector;
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server containing the users currently connected.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    // Split string by all spaces.
    delimeterStringSplitter(receivedUserNamesVector, receiveBuffer, ' ');

    printLine();
    cout << "CONNECTED USERS:" << endl;
    for (size_t i = 0; i < receivedUserNamesVector.size(); i++) { cout << receivedUserNamesVector[i] << endl; }
    printLine();

    // Zero out("clean") buffers.
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);
}

void getAndPrintDirectlyConnectedServers(string input) {
    char sendBuffer[input.length() + 1];
    char receiveBuffer[XXLARGEBUFFERSIZE];
    vector<string> directlyConnectedServersVector;

    // Zero out("clean") buffers.    
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);

    strcpy(sendBuffer, input.c_str());

    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    // Split string by all ";".
    delimeterStringSplitter(directlyConnectedServersVector, receiveBuffer, ';');

    printLine();
    cout << "DIRECTLY CONNECTED SERVERS:" << endl;
    for (size_t i = 0; i < directlyConnectedServersVector.size(); i++) { cout << directlyConnectedServersVector[i] << endl; }
    printLine();
}

// Use API calls MSG ALL or MSG to send a message to a specific person or all persons.
void sendMessage(string message, bool sendPrivate) {
    if (!sendPrivate) { message = "MSG ALL " + message; }
    if (sendPrivate) { message = "MSG " + message; }

    char sendBuffer[message.length() + 1];

    // Zero out("clean") buffer.
    memset(sendBuffer, 0, sizeof sendBuffer);

    strcpy(sendBuffer, message.c_str());

    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
}

// Use API calls RECV to enter a receiving mode.
// Enter receive mode to receive messages from other connected users.
// The user must enter a time in seconds that he wiches to be in receive mode.
// After the receive mode, the user is free to make other commands.
void receiveMode(int timeOut) {
    cout << "Entering recieve mode for " << timeOut << " seconds." << endl;
    // Create the correct string to send to server.
    char sendBuffer[MINBUFFERSIZE] = "RECV";
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Zero out("clean") buffer.
    memset(sendBuffer, 0, sizeof sendBuffer);

    time_t timeStarted, timeElapsed;
    time(&timeStarted);
    // Loop will run until the timeout is reached.
    while (true) {
        if(difftime(time(&timeElapsed), timeStarted) >= timeOut) {
            cout << "Exiting receive mode.." << endl;
            return;
        }

        char receiveBuffer[XXLARGEBUFFERSIZE];
        int bytesReceieved = recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, MSG_DONTWAIT);
        // Message is written out if something is received from the server.
        if(bytesReceieved > 0) { cout << receiveBuffer << endl; }
        memset(receiveBuffer, 0, sizeof receiveBuffer);
    }
}

// Use API calls LEAVE to leave the server and exit the program in a safe way.
bool leaveServerAndQuit() {
    string answer = "";

    while (true) {
        cout << "Are you sure you want to leave? (y/n)" << endl;
        cout << user << ": ";
        cin >> answer;
        if (answer == "y") {
            char sendBuffer[MINBUFFERSIZE] = "LEAVE";
            send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
            return true;
        }
        else if (answer == "n"){ return false; }
        else { cout << "Invalid command. Try again." << endl; }
    }
    return false;
}

bool connectToServer() {
    cout << "Connecting to chat server..." << endl;
    // Create the address of the socket
    struct sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;

    socketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    socketAddress.sin_port = htons(LISTENINGSERVERPORT);
    if(connect(socketDescriptor, (struct sockaddr *)&socketAddress, sizeof socketAddress) == 0) { return true;}
    return false;
}

void connectServerToServer(string input) {
    char sendBuffer[input.size()];
    memset(sendBuffer, 0, sizeof sendBuffer);
    strcpy(sendBuffer, input.c_str());
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
}

void fetchHashString(string input) {
 
    char sendBuffer[input.size() +1];
    memset(sendBuffer, 0, sizeof sendBuffer);
    strcpy(sendBuffer, input.c_str());

    char receiveBuffer[XLARGEBUFFERSIZE];
    memset(receiveBuffer, 0, sizeof receiveBuffer);
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server containing the server id and print out.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    printLine();
    fprintf(stderr, "FETCH hash \n%s\n", receiveBuffer);
    printLine();
}

void cmd(string input){
    char sendBuffer[input.size()];
    memset(sendBuffer, 0, sizeof sendBuffer);
    strcpy(sendBuffer, input.c_str());
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0); 
}

// Run the chatroom by calling functions that use the chat server API.
void runChatRoom(string input) {
    user = input;
    bool wrongInput;
    vector<string> inputCommands;
    printLoggedInMessage();
    printCommands();


    // Run loop until user chooses the esc option to leave the chatserver.
    while (true) {
        wrongInput = false;
        cout << user << ": ";
        getline(cin, input);

        // Use split function to splitting string on commas and placing them in order in the vector inputCommands.
        delimeterStringSplitter(inputCommands, input, ',');

        // Various function calls depending on input from user.
        if (inputCommands[0] == "MAN" || inputCommands[0] == "HELP") { printCommands(); }
        else if (inputCommands[0] == "SND") { 
            if (inputCommands.size() == 2) { sendMessage(inputCommands[1], false); }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "SNDPR") { 
            if (inputCommands.size() == 3) { sendMessage(inputCommands[1], true); }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "LST") { 
            if (inputCommands.size() == 1) { getAndPrintServerUsers();  }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "SID") { 
            if (inputCommands.size() == 1) { getAndPrintServerId(); }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "ESC") {
            if (inputCommands.size() == 1) { if (leaveServerAndQuit()) { break; } }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "RECV") { 
            if(inputCommands.size() == 2 && atoi(inputCommands[1].c_str()) > 0) { receiveMode(atoi(inputCommands[1].c_str())); } 
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "LISTSERVERS") { 
            if (inputCommands.size() == 1) { getAndPrintDirectlyConnectedServers(input);  }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "LISTROUTES") { 
            if (inputCommands.size() == 1) { cout << "TODO: Implement LISTROUTES" << endl;   }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "CMD") { 
            if (inputCommands.size() == 5 || inputCommands.size() == 4) { cmd(input); }
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "SCONNECT") { 
            if(inputCommands.size() == 3) { connectServerToServer(input); } 
            else { wrongInput = true; }
        }
        else if (inputCommands[0] == "FETCH") { 
            if (inputCommands.size() == 2) { 

                fetchHashString(input);
            }
            else { wrongInput = true; }
        }
        else { wrongInput = true; }

        if(wrongInput) { cout << "Invalid input, try again(input man/help to se list of commands)." << endl; }

        // After each command, the vector needs to be cleared.
        inputCommands.clear();
    }
}
