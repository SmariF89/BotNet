# BOTNET

## BUILDING

    This project was developed and tested on Ubuntu 18.04.

    In order to build the project into a runnable form use the provided script, runServer.sh.
    This script compiles both the server and client programs and finally starts an instance of
    the server.
    Example command: "./runServer.sh 4026 42"
    It takes two arguments:
        1. TCP port for server connections. The server instance uses this port to connect to other
           servers in the BotNet as well as receiving connections from other servers of the BotNet.
        2. UDP port for information requests. The server instance uses this port to receive queries
           regarding servers that are directly connected to it.

    To run extra instances of the server, simply type "./tsamvgroup26 <TCP_Port> <UDP_Port>.
    The client takes no argument an can simply be run by typing "./c_and_client".

## USING

    An instance of our server can be connected to by client c_and_client which connects via the hardcoded
    client port of 1337. The client serves as the botmaster and he alone is able to control the server
    instance it is connected to.

    First, run an instance of the server by either using the script, described above, or executing an instance
    of the server directly, as is also described above. Then run an instance of the client and it should auto-
    matically connect to the server instance. If you have run a few instances of the server, the client will
    connect to the first one which was run.

    In order to connect an instance of a server to another server, connect a client to it and, from the client,
    enter the command "SCONNECT,<IP>,<PORT>". The server will attempt to connect to the provided IP with the
    provided port number.

    The client is able to fetch hashes from other servers in the BotNet. In the client, simply type the command
    "CMD,<TOSERVERID>,<FROMSERVERID>,FETCH,<INDEX(1-5)>" and a response should be received shortly.

    The server is also a fully functional chat server. Multiple connected clients are able to exchange messages.

## DESIGN DECISIONS

    When a server has been connected to our server or when we have made connection to another server a CMD is
    immediately sent to the server in question, enquiring it for its id. As soon as its ID is received it saved
    and kept in our bookings (vector of connected servers).
    We decided to use the pattern described on Piazza, thread #333 (https://piazza.com/class/jkxwibf75z3527?cid=333)
    to obtain IDs of connected servers. There seemed to be consensus on following that pattern so we did too. We also
    send a corresponding RSP when receiving such an ID request.

    We decided to keep track of directly connected server by having a designated struct which has fields containing
    each server's socket file descriptor, port and IP address. Furthermore, there is a vector which holds an instance of
    these structs for every directly connected server. That way it is easy to maintain our connections to other nodes
    in the net.

    An instance of our server makes a periodic (every 1 1/2 minute) task of:

        a) Disconnecting inactive servers. It loops through the vector of servers and checks
           their timestamps. Servers that have not sent a KEEPALIVE command for more than 1 1/2
           minute are disconnected.

        b) Sending KEEPALIVE commands to directly connected servers. That way it ensures that it
           will not be disconnected due to inactivity.

## KNOWN ISSUES

    UDP requests will not be answered.
    FETCH commands sent to directly connected servers will not be received by the client.
    The client can however use the FETCH command (not by using CMD) to get his own hashes.

## IMPORTANT NOTE

    This project is not fully implemented due to time constraints.
    However, below is a description of what would have been implemented and how it would have been done.

    // --- Description on implementations --- //

        // ==== Implementation for routing table. ==== //

        // The server will have a global vector that contains the current
        // routing table for the botnet server. This vector will contain the struct RouteNode
        // which contains an id for a node(the group name), the number of hops to that node from the source node,
        // and a vector containing the path to that node from the source node.

        // A thread will be made that runs the function that maintains and updates
        // the routing table for the botnet from the base node(our server).
        // These updates will happen every 10 minutes.
        // When updating the routing table this function will call another
        // function that updates the routing table.

        // The pseudo code for the implementation of the function is shown below:

        // ---------- First function(the thread function) ---------------------------------------------------------
           // A while(true loop)
                // 1. Wait for 10 minutes.
                // 2. Update the routing table by calling the second function.
                // 3. (If nearby routers drop for some reason, this function will be notified and
                //    update the routing table by calling the second function, and initialize the
                //    waiting time for the next update.


        // ---------- Second function ---------------------------------------------------------
            // Initialize a map of type <string, vector<string> > to hold the routes from each node
            // after splitting string up.

            // A for loop that loops through the nearest nodes. So for each nearest node(1 hop) from the source node:
                // Send a command to retrieve its routing table.
                // Use a helper function to split the string correctly and place each access point
                // in the map(described earlier), the map is sent call by reference into the function.
                // Another for loop that iterates through the map and for each node in the map:
                    // Another for loop that runs through the current
                    // rout table vector(which is global) and for each node:
                        // Check if the node in the map being examined exists in the current
                        // rout table vector.
                            // If it does and if it has a shorter route to that node,
                                // Update the route for to this node.
                            // else if it does not,
                                // Add the node to the route table vector.
                                // Call the second function again with the new node. (recursion)
                            // else continue in the loop. (should stop the recursion eventually)

## CREDITS

    This BotNet server/client was designed and implemented by:

        Snorri Arinbjarnar          - snorria16
        Smári Freyr Guðmundsson     - smarig16
        Þórir Ármann Valdimarsson   - thorirv15
