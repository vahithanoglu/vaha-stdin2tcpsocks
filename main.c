/*
 * Copyright (C) 2021 Vahit Hanoglu
 *
 * This file is part of vaha-stdin2tcpsocks.
 *
 * vaha-stdin2tcpsocks is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vaha-stdin2tcpsocks is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with vaha-stdin2tcpsocks. If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CONN_COUNT 4
#define BUFFER_SIZE 1024

// global variables (TODO: introduce mutex for thread safety)
int keepListen;
int enableHttp;
int connFDs[MAX_CONN_COUNT];
char *httpHeaders = "HTTP/1.1 200 OK\n"
        "Content-type: application/octet-stream\n"
        "Cache-Control: no-cache\n"
        "Connection: close\n\n";

void printfAndFlush(const char *str, ...) {
    va_list args;
    va_start(args, str);
    vprintf(str, args);
    va_end(args);
    fflush(stdout);
}

void *acceptIncomingConnections(void *vargp) {
    int i, writeN, connFD;
    char IP[16];
    socklen_t addrLen;
    struct sockaddr_in clientAddr;
    int headersLen = strlen(httpHeaders);
    int *sockFD = (int *) vargp;

    while (keepListen) {
        addrLen = sizeof (clientAddr);
        connFD = accept(*sockFD, (struct sockaddr*) & clientAddr, &addrLen);

        if (-1 == connFD && keepListen) {
            printfAndFlush("Cannot accept incoming connection.\n");

        } else if (keepListen) {
            inet_ntop(AF_INET, &clientAddr.sin_addr, IP, sizeof (IP));

            for (i = 0; i < MAX_CONN_COUNT; i++) {
                if (0 == connFDs[i]) {
                    printfAndFlush("Accepting incoming connection, connFD(%d -> %s:%hu).\n", connFD, IP, clientAddr.sin_port);

                    // if http is enable, then first send the http response headers
                    if (enableHttp) {
                        writeN = 0;
                        while (writeN < headersLen) {
                            writeN += write(connFD, httpHeaders + writeN, headersLen - writeN);
                        }
                    }

                    connFDs[i] = connFD;
                    break;
                }
            }

            if (MAX_CONN_COUNT == i) {
                printfAndFlush("Cannot accept incoming connection, connFD(%d -> %s:%hu); Because MAX_CONN_COUNT=%d is reached.\n", connFD, IP, clientAddr.sin_port, MAX_CONN_COUNT);
                close(connFD);
            }
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    int i, port, readN, writeN, sockFD;
    char buffer[BUFFER_SIZE];
    pthread_t threadId;
    struct sockaddr_in serverAddr;

    // ignore broken pipe signal (in our case for tcp sockets)
    signal(SIGPIPE, SIG_IGN);

    if (3 != argc && 4 != argc) {
        printfAndFlush("Command line argument(s) is/are missing.\n"
                "Generic Usage:\n"
                "\t%s {IPv4_TO_BIND} {PORT_TO_BIND} [--enable-http]\n"
                "Example Usages:\n"
                "\t%s 127.0.0.1 9095\n"
                "\t%s 0.0.0.0 8080 --enable-http\n"
                "\t%s 192.168.1.5 80 --enable-http\n", argv[0], argv[0], argv[0], argv[0]);
        exit(1);
    }

    // check and validate the port number
    port = atoi(argv[2]);
    if (1024 >= port || port >= 49151) {
        printfAndFlush("Cannot use '%s' as the {PORT_TO_BIND} value. Valid user ports are in the range of (1024-49151) as defined in the RFC6335.\n"
                "You can check the following documents to decide what port to use:\n"
                "- https://tools.ietf.org/html/rfc6335\n"
                "- https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt\n", argv[2]);
        exit(1);
    }

    // create address(IP and Port) for the socket
    memset(&serverAddr, '\0', sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    i = inet_pton(AF_INET, argv[1], &(serverAddr.sin_addr));
    if (1 != i) {
        printfAndFlush("Cannot use '%s' as the {IPv4_TO_BIND} value. It is not a valid IPv4 address.\n", argv[1]);
        exit(1);
    }

    // check whether the http headers are enabled
    enableHttp = (4 == argc && 0 == strcmp("--enable-http", argv[3])) ? 1 : 0;

    // create a socket to listen (TCP)
    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockFD) {
        printfAndFlush("Cannot create a server socket to listen incoming connections. Exiting the app...\n");
        exit(1);
    }

    // prevent port binding problems in case we perform consecutive runs
    i = 1;
    setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &i, sizeof (i));


    // bind the socket to the address
    if (0 != bind(sockFD, (struct sockaddr*) &serverAddr, sizeof (serverAddr))) {
        printfAndFlush("Cannot bind the server socket to %s:%d. Exiting the app...\n", argv[1], port);
        exit(1);
    }

    // start listening to the socket (backlog size of 2)
    if (0 != listen(sockFD, 2)) {
        printfAndFlush("Cannot start listening on the server socket. Exiting the app...\n");
        exit(1);
    }

    keepListen = 1;
    if (0 != pthread_create(&threadId, NULL, acceptIncomingConnections, (void *) &sockFD)) {
        printfAndFlush("Cannot create thread to accept incoming connections. Exiting the app...\n");
        keepListen = 0;

    } else {
        printfAndFlush("Execution started succesfully. Listening on the server socket bind to %s:%d...\n", argv[1], port);
        if (enableHttp) {
            printfAndFlush("'--enable-http' was set. Enabling the HTTP response headers while writing to client connections.\n");
        }
    }

    while (keepListen) {
        readN = read(STDIN_FILENO, buffer, BUFFER_SIZE);
        if (0 >= readN) {
            printfAndFlush("Cannot read from standard input. Exiting the app...\n");
            keepListen = 0;
        }

        for (i = 0; i < MAX_CONN_COUNT && keepListen; i++) {
            if (0 != connFDs[i]) {
                writeN = write(connFDs[i], buffer, readN);
                if (0 >= writeN) {
                    printfAndFlush("Cannot write to client connection. Terminating connFD(%d).\n", connFDs[i]);
                    close(connFDs[i]);
                    connFDs[i] = 0;
                }
            }
        }
    }

    // close open connections
    for (i = 0; i < MAX_CONN_COUNT; i++) {
        if (0 != connFDs[i]) {
            printfAndFlush("Terminating client connection, connFD(%d).\n", connFDs[i]);
            close(connFDs[i]);
        }
    }

    // close server socket
    printfAndFlush("Shutting down the server socket, sockFD(%d).\n", sockFD);
    shutdown(sockFD, SHUT_RDWR);

    // wait for thread to terminate
    pthread_join(threadId, NULL);

    exit(0);
}
