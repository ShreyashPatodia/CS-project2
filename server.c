/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                               Server Module                               *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 25/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>

#include "codemaker.h"

#include "log.h"

#define BACKLOG 30  // the max number of connect()'s to queue at once
                    // just needs to be more than 20!

#define EVER (;;)   // macro for infinite loops
                    // usage: for EVER { ... } --> for (;;) { ... }

// command line options
typedef struct {
    char* port;
    char* code;
} opts_t;
opts_t load_options(int argc, char** argv);

// helper function for setting up a new listener socket
int listen_to(char* port);

// helper function to block SIGINT & SIGTERM for this pthread and all
// new pthreads it creates
// call this after starting a signal handling cleaner thread :)
void block_signals();

// entry point for cleanup thread, which idles until a signal is received
// (it's important that it has its own thread so that it is guaranteed to
//  not be holding any mutex when an asynchronous signal is recieved)
void* start_cleaner(void* p);

// entry point for client-handling codemaker thread
// takes pointer to a (malloced) struct codemaker_input (and frees it when done)
void* start_codemaker(void* p);

// for input to codemaker thread start function
struct codemaker_input {
    int breaker;    // the socket for communicating with a codebreaker
    char* code;     // the code to use as a string (or NULL for a random code)
};


/* * * * * * * * * * ENTRY POINT FOR SERVER * * * * * * * * * */

int main(int argc, char** argv){

    // first, parse command line options
    opts_t options = load_options(argc, argv);

    // begin logfile
    start_log("log.txt");

    // next, attempt to listen on specified port
    int welcome_fd = listen_to(options.port);

    // once welcome_fd has been set and the logs are open,
    // create a new NON-LOGGING pthread to handle termination signals and
    // clean up the server's resources
    pthread_t cleaner;
    pthread_create(&cleaner, NULL, start_cleaner, NULL);

    // for this thread and all subsequently created threads,
    // make sure we ignore termination signals so that the cleanup thread
    // can handle those
    block_signals();
    
    for EVER {
        struct sockaddr_storage client;
        socklen_t client_size = sizeof client;

        // is there a new connection ready?
        log_block_read_test(welcome_fd);

        // either way, wait for a new connection to come in!
        int fd = accept(welcome_fd, (struct sockaddr*)&client, &client_size);
        if(fd < 0){
            // it's possible that these errors are because we closed the
            // welcoming socket...

            perror("error creating client socket");
            // we're just going to have to skip this connection request, sorry!
            continue;
        }

        // we have a new connection!
        log_connection(fd);

        // give this new client file descriptor and the code to a new thread
        struct codemaker_input *input = malloc(sizeof *input);
        *input = (struct codemaker_input) {fd, options.code};

        pthread_t client_handler;
        pthread_create(&client_handler, NULL, start_codemaker, input);
    }
}

// entry point for client-handling codemaker thread
// takes pointer to a (malloced) struct codemaker_input (and frees it when done)
void* start_codemaker(void* p){
    struct codemaker_input* input = (struct codemaker_input*)p;

    // play the game!
    play_codemaker(input->breaker, input->code);

    // once the game is finished, this socket is no longer needed!
    close(input->breaker);
    
    // this input was nicely malloced for us, but now we should free it
    free(input);

    // NOW update the server stats about this thread

    return NULL;
}

/* * * * * * * * * * EXIT POINT FOR SERVER * * * * * * * * * */

// simle signal handler for termination and interrupt signals
// just cleans up and exits
void end_handler(int sig){

    printf("\nserver closing...\n");

    // write final values to log file too
    close_log();
    
    // no need to clean up our sockets, the OS will take care of that
    // and they will get a nice 'disconnect' error through their sockets
    // if they happen to be still waiting for a server response

    // the welcome socket as well, no need to close it as we are using
    // SO_REUSEADDR when we open it so next time we start the server there will
    // be no problems!

    printf("server off!\n");
    exit(EXIT_SUCCESS);
}

// entry point for cleanup thread, which idles until a signal is received
// (it's important that it has its own thread so that it is guaranteed to
//  not be holding any mutex when an asynchronous signal is recieved)
void* start_cleaner(void* p){

    // redirect ^C and SIGTERM to finish logs and exit

    struct sigaction action;
    action.sa_handler = end_handler;
    sigemptyset(&action.sa_mask);   // clear the signal blocking mask
    action.sa_flags = 0;            // and the flags

    sigaction(SIGINT, &action, NULL);   // using sigaction rather than signal
    sigaction(SIGTERM, &action, NULL);  // because apparently it's way better

    // idle while we're not handling a signal!

    sigset_t ss;        // signals to block while we suspend
    sigemptyset(&ss);   // (none)
    for EVER {
        sigsuspend(&ss);
    }
}

// helper function to block SIGINT & SIGTERM for this pthread and all
// new pthreads it creates
// call this after starting a signal handling cleaner thread :)
void block_signals(){
    int n;

    // initialise empty signal set
    sigset_t ss;
    n = sigemptyset(&ss);
    if(n < 0){
        perror("error initialising signal set");
    }

    // add necessary signals to set
    n = sigaddset(&ss, SIGINT);
    if(n < 0){
        perror("error adding SIGINT");
    }
    n = sigaddset(&ss, SIGTERM);
    if(n < 0){
        perror("error adding SIGTERM");
    }

    // block these signals for this pthread and all new pthreads it creates
    n = pthread_sigmask(SIG_BLOCK, &ss, NULL);
    if(n < 0){
        fprintf(stdout, "error blocking signals with pthread_sigmask\n");
    }
}

/* * * * * * * * * * HANDLING COMMAND LINE INPUT * * * * * * * * * */

// print usage info then exit
void usage_exit(char* name);

// parses command line inputs into our options structure
opts_t load_options(int argc, char** argv){

    opts_t options;

    if(argc < 2){
        // not enough arguments!
        usage_exit(argv[0]);

    } else {
        options.port = argv[1];
    }

    if(argc > 2){
        options.code = argv[2];
    } else {
        options.code = NULL;
    }

    return options;
}

// print usage info then exit
void usage_exit(char* name){
    fprintf(stderr, "usage: %s port [code]\n", name);
    fprintf(stderr, "  port: server port to listen on\n");
    fprintf(stderr, "  code: (optional) secret code to use for each client "
                    "(default: random)\n");

    exit(EXIT_FAILURE); // as promised
}

/* * * * * * * * * * BINDING AND LISTENING TO PORT * * * * * * * * * */

// helper function for setting up a new listener socket
int listen_to(char* port){
    
    // set up options struct addrinfo for getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // when host is also NULL in getaddrinfo,
                                    // this fills in loopback address for us!

    // get address info for this port

    struct addrinfo *ports; // pointer to first result
    
    int status = getaddrinfo(NULL, port, &hints, &ports);
    if (status != 0){
        fprintf(stderr, "error finding port %s!", port);
        fprintf(stderr, " %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    // if that works okay, time to open a socket!

    int fd = socket(ports->ai_family, ports->ai_socktype, 0);
    if(fd < 0){
        perror("error creating socket");
        exit(EXIT_FAILURE);
    }

    // set socket option SO_REUSEADDR to make that okay
    // (if a recently closed server was using this port and some of it is
    //  still hanging around, we still want to be able to use this port)

    int yes=1;
    if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1){
        perror("error setting socket option for reusing address");
        exit(EXIT_FAILURE);
    } 

    // bind it

    if(bind(fd, ports->ai_addr, ports->ai_addrlen) < 0){
        perror("error binding to port");
        exit(EXIT_FAILURE);
    }

    // and finally, listen to it!

    if(listen(fd, BACKLOG)){
        perror("error listening on port");
        exit(EXIT_FAILURE);
    }

    // once we're done, make sure we free the list of results from getaddrinfo

    freeaddrinfo(ports);

    // and return the file descriptor we're looking for!

    return fd;
}