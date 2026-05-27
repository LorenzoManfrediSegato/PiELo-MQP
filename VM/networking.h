#pragma once
#include <time.h>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <iostream>
#include "vm.h"

namespace PiELo {

    extern int socketfd;
    extern addrinfo *routerinfo;

    // When true, the VM runs standalone with no router: initNetworking skips the
    // socket handshake (robotID is forced to 0) and checkForMessage /
    // broadcastVariable become no-ops. Enabled by setting the PIELO_OFFLINE env
    // var. Lets a single robot run deterministically without the router (used by
    // the test harness and handy for isolated/RL rollouts). NOTE: cross-robot
    // reactivity and stigmergy merge only happen on the network path, so they are
    // NOT exercised in offline mode -- cover those with multi-robot tests.
    extern bool offline;

    struct Message {
        timestamp_t variableLastUpdated;
        int robotID;
        long senderX;
        long senderY;
        long senderZ;
        // TODO: Update this or make it the law
        char variableName[100];
        VariableData data;
        bool isStigmergy;
    };

    // Setup sockets and things. Returns 0 on success
    int initNetworking(void);

    // Clean up stored networking info
    void cleanupNetworking(void);

    // Broadcast a variable data
    timestamp_t broadcastVariable(std::string name, Variable v);

    // Check for a message and update a variable or rebroadcast own value if necessary
    void checkForMessage(void);
}