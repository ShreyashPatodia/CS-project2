/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                               Client Module                               *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 20/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "codebreaker.h"

// struct for holding command line options
typedef struct {
    char* port;
    char* host;
} opts_t;

// helper function for parsing command line options into a useable
// opts_t structure
opts_t load_options(int argc, char** argv);

// helper function for setting up a socket connection to
// a port at a host address (ip address or domain name)
//
// returns a socket
int connect_to(char* host, char* port);

/* * * * * * * * * * ENTRY POINT FOR CLIENT * * * * * * * * * */

int main(int argc, char** argv){

    // first, parse command line options
    opts_t options = load_options(argc, argv);

    // next, attempt to connect to server
    int fd = connect_to(options.host, options.port);
    
    // finally, begin playing the game!
    play_codebreaker(fd);

    // once the game is over, clean up and go home
    close(fd);
    return EXIT_SUCCESS;
}

/* * * * * * * * * * HANDLING COMMAND LINE INPUT * * * * * * * * * */

// print usage info then exit
void usage_exit(char* name){
    fprintf(stderr, "usage: %s host port\n", name);
    fprintf(stderr, "  host: hostname or IP address of server\n");
    fprintf(stderr, "  port: server port to connect with\n");

    exit(EXIT_FAILURE); // as promised
}

// helper function for parsing command line options into a useable
// opts_t structure
opts_t load_options(int argc, char** argv){

    opts_t options;

    if(argc < 3){
        // not enough arguments!
        usage_exit(argv[0]);

    } else {
        options.host = argv[1];
        options.port = argv[2];
    }

    return options;
}

/* * * * * * * * * * CONNECTING TO SERVER * * * * * * * * * */

// helper function for setting up a socket connection to
// a port at a host address (ip address or domain name)
//
// returns a socket
int connect_to(char* host, char* port){
    
    // set up options struct addrinfo for getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // get address info for this host and port

    struct addrinfo *servers;   // pointer to first result
    
    int status = getaddrinfo(host, port, &hints, &servers);
    if (status != 0){
        fprintf(stderr, "error finding address %s:%s!",
            host, port);
        fprintf(stderr, " %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    // if that works okay, time to open a socket!

    int fd = socket(servers->ai_family, servers->ai_socktype, 0);
    if(fd < 0){
        perror("error creating socket");
        exit(EXIT_FAILURE);
    }

    // and finally, make the connection!

    if(connect(fd, servers->ai_addr, servers->ai_addrlen) < 0){
        perror("error connecting");
        exit(EXIT_FAILURE);
    }

    // once we're done, make sure we free the list of results from getaddrinfo
    freeaddrinfo(servers);

    // and return the file descriptor we're looking for!

    return fd;
}
