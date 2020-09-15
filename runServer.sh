#!/bin/bash
g++ tsamvgroup26.cpp -o tsamvgroup26 -lpthread
g++ c_and_c_client.cpp -o c_and_c_client
echo "Done building and compiling client and server. Now running server.."
./tsamvgroup26 $1 $2
