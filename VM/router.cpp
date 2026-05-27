#include "networking.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>

static const char *PORT = "5005";
static const int MAXBUFLEN = 512;

int currentID = 0;

// client info
struct ClientInfo
{
    sockaddr_in addr;
    socklen_t addrLen;
};

// check if two sockaddr_in are the same (by IP + port)
bool sameAddress(const sockaddr_in &a, const sockaddr_in &b)
{
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) &&
           (a.sin_port == b.sin_port) &&
           (a.sin_family == b.sin_family);
}

int main()
{
    // resolve address info for binding
    struct addrinfo hints, *servinfo;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_flags = AI_PASSIVE;    // binding/listening

    int rv = getaddrinfo(nullptr, PORT, &hints, &servinfo);
    if (rv != 0)
    {
        std::fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // create socket
    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1)
    {
        perror("router: socket");
        return 1;
    }

    // allow reuse of the port
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1)
    {
        perror("router: setsockopt SO_REUSEPORT");
        close(sockfd);
        return 1;
    }

    // bind to the port
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        perror("router: bind");
        close(sockfd);
        freeaddrinfo(servinfo);
        return 1;
    }
    freeaddrinfo(servinfo);

    std::printf("Router listening on port %s...\n", PORT);

    std::vector<ClientInfo> clients; // We'll store addresses here

    int status;
    pid_t child_pid = fork();
    bool goSignalSent = false;

    // Test-only deterministic start: if PIELO_GO_AFTER=N is set, the router
    // broadcasts the go signal the instant the Nth client registers -- no stdin,
    // no waiting on the forked child. Unset (== 0) preserves the original
    // press-Enter-to-go behavior exactly.
    int goAfter = 0;
    if (const char *env = std::getenv("PIELO_GO_AFTER")) goAfter = std::atoi(env);

    if (child_pid == -1) {
        perror("fork");
        exit(-1);
    }

    if (child_pid == 0) {
        // this is the child
        // Just wait for some input and then exit
        std::string input;
        std::cout << "Child alive!" << std::endl;
        std::cin >> input;
        std::cout << "Child dead!" << std::endl;
        exit(0);
    }
    else {
        // main receive-forward loop (parent)
        while (true)
        {
            struct sockaddr_in theirAddr;
            socklen_t addrLen = sizeof(theirAddr);
            char buf[MAXBUFLEN];

            // std::cout << "waiting" << std::endl;;
            int wpid = waitpid(child_pid, &status, WNOHANG);
            // If waitpid returns 0, it means there are no children alive (go signal has already been sent)
            if (wpid != 0) {
                // std::cout << "wpid " << wpid << " status " << status << std::endl;
                if (wpid == -1) {
                    if (errno != ECHILD) {
                        perror("waitpid");
                        exit(-1); // Something actually went wrong
                    }
                } else if (goAfter == 0) {
                    // stdin-driven go: the child read input/EOF and exited. Skipped
                    // in PIELO_GO_AFTER mode, where go is purely registration-counted
                    // (otherwise an EOF on a detached stdin fires go prematurely).
                    std::cout << "Sending go signal!" << std::endl;
                    goSignalSent = true;
                    // The child just died and the go signal should be sent
                    for (auto &client : clients)
                    {
                        ssize_t sentBytes = sendto(sockfd, "", 0, 0,
                                                (struct sockaddr *)&client.addr, client.addrLen);
                        if (sentBytes == -1)
                        {
                            perror("router: sendto");
                        }
                    }
                }
            }
            
            // Set timeout to 10 microseconds
            timeval read_timeout;
            read_timeout.tv_sec = 0;
            read_timeout.tv_usec = 10;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) != 0) {
                perror("setsockopt receive timeout");
                return 1;
            }

            // block until packet received
            ssize_t numBytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                                        (struct sockaddr *)&theirAddr, &addrLen);
            if (numBytes == -1)
            {   
                // EAGAIN means nothing was available
                if(errno != EAGAIN) perror("router: recvfrom");
                continue; // keep trying
            } 
            buf[numBytes] = '\0'; // null-terminate for printing

            // check if client is known
            bool found = false;
            for (auto &client : clients)
            {
                if (sameAddress(client.addr, theirAddr))
                {
                    found = true;
                    break;
                }
            }

            // if unknown add to list
            if (!found)
            {
                ClientInfo ci;
                ci.addr = theirAddr;
                ci.addrLen = addrLen;
                clients.push_back(ci);

                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &theirAddr.sin_addr, ipStr, sizeof(ipStr));
                std::printf("New client registered: %s:%d, id %d\n",
                            ipStr, ntohs(theirAddr.sin_port), currentID);

                ssize_t sentBytes = sendto(sockfd, &currentID, sizeof(currentID), 0,
                                        (sockaddr*) &theirAddr, sizeof(sockaddr_in));
                if (sentBytes == -1)
                {
                    perror("router: sendto");
                }
                currentID++;

                if (goSignalSent) {
                    std::cout << "Sent go signal as they are late." << std::endl;
                    sentBytes = sendto(sockfd, "", 0, 0,
                                            (struct sockaddr *)&theirAddr, sizeof(sockaddr_in));
                    if (sentBytes == -1)
                    {
                        perror("router: sendto");
                    }
                }

                // Deterministic start (test-only): once N clients have registered,
                // broadcast the go signal to all of them at once.
                if (goAfter > 0 && !goSignalSent && currentID >= goAfter) {
                    std::cout << "PIELO_GO_AFTER=" << goAfter
                              << " reached; sending go signal!" << std::endl;
                    goSignalSent = true;
                    for (auto &client : clients) {
                        ssize_t sent = sendto(sockfd, "", 0, 0,
                                              (struct sockaddr *)&client.addr, client.addrLen);
                        if (sent == -1) perror("router: sendto");
                    }
                }
                if (numBytes == 0) continue; // This is the init ping
            }

            // print out message
            char senderIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &theirAddr.sin_addr, senderIp, sizeof(senderIp));
            if (numBytes != sizeof(PiELo::Message)) {
                printf("Got incorrectly sized packet from %s:%d. Got %ld bytes.\n", senderIp, ntohs(theirAddr.sin_port), numBytes);
                continue;
            }
            PiELo::Message *msg = (PiELo::Message*) buf;
            std::printf("Received update to variable %s from %s:%d.\n",
                        msg->variableName, senderIp, ntohs(theirAddr.sin_port));

            // forward the message to all clients
            for (auto &client : clients)
            {
                // if (sameAddress(client.addr, theirAddr)) continue;

                ssize_t sentBytes = sendto(sockfd, buf, numBytes, 0,
                                        (struct sockaddr *)&client.addr, client.addrLen);
                if (sentBytes == -1)
                {
                    perror("router: sendto");
                }
            }
            
        }
    }

    close(sockfd);
    return 0;
}
