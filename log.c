/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                               Logging Module                              *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 24/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "log.h"

// struct for maintaining server running statistics
struct server_stats{
    int connections;        // the number of successful connections logged
    int disconnections;     // the number of client disconnections logged
    
    int winning_clients;    // the number of winning messages sent
    int losing_clients;     // the number of losing  messages send
    
    int mutex_blocks_log;   // the number of blocks on logfile mutex
    int mutex_blocks_stat;  // the number of blocks on stat mutex
    int socket_blocks;      // approximate number of blocks on sockets logged
};

// global statistics structure and associated mutex
// (we COULD have finer-grain locks but under load tests these barely block)
static struct server_stats stats;   // automatically zeroed (global)
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// global logfile file descriptor and associated mutex
static FILE* logfile = NULL;
pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;

// NOTE: to avoid deadlock, do not try to log the logfile if you already
// have a lock on the stats mutex

/* * * * * * * * * * CONCURENCY MEASUREMENT FUNCTIONS * * * * * * * * * */

// wrapper function to obtain mutex lock on logfile (and record blocks)
void lock_logfile();

// wrapper function to release mutex lock on logfile
void unlock_logfile();

// wrapper function to obtain mutex lock on stats struct (and record blocks)
void lock_stats();

// wrapper function to release mutex lock on stats struct
void unlock_stats();



// wrapper function to obtain mutex lock on logfile (and record blocks)
void lock_logfile(){
    int n = pthread_mutex_trylock(&logfile_mutex);

    if(n != 0){
        // we would have blocked, so we returned!

        // make sure we actually block
        pthread_mutex_lock(&logfile_mutex);

        // okay, we're into the logfile!
        // now make sure we can also update the stats
        lock_stats();
        stats.mutex_blocks_log++;
        unlock_stats();
    }
}

// wrapper function to release mutex lock on logfile
void unlock_logfile(){
    // all we need to do is unlock
    pthread_mutex_unlock(&logfile_mutex);
}

// wrapper function to obtain mutex lock on stats struct (and record blocks)
void lock_stats(){
    // lock the stats mutex
    int m = pthread_mutex_trylock(&stats_mutex);

    if(m != 0){
        // we would have blocked! block
        pthread_mutex_lock(&stats_mutex);

        // update the stats mutex
        stats.mutex_blocks_stat++;
    }
}

// wrapper function to release mutex lock on stats struct
void unlock_stats(){
    // nothing to worry about here, just unlock
    pthread_mutex_unlock(&stats_mutex);
}


/* * * * * * * * * * I/O BLOCKING MEASUREMENT FUNCTIONS * * * * * * * * * */

// log a single blocking operation on socket
void log_socket_block(){

    lock_stats();

    stats.socket_blocks++;

    unlock_stats();
}

// test if this socket descriptor is about to block
// on us for reading/accepting!
void log_block_read_test(int fd){

    struct timeval t = { 0, 0 }; // zero timeval --> poll sockets, don't wait

    fd_set fds;
    FD_ZERO(&fds); // a new empty fd_set

    // add OUR fd to the set
    FD_SET(fd, &fds);

    // update the set to contain only 'ready' file descriptors
    select(fd+1, &fds, NULL, NULL, &t);

    // is our filedescriptor not in the ready set?
    if( ! FD_ISSET(fd, &fds)){
        // no its not! we will block!
        log_socket_block();
    }
}

// test if this socket descriptor is about to block
// on us for writing!
void log_block_write_test(int fd){

    struct timeval t = { 0, 0 }; // zero timeval --> poll sockets

    fd_set fds;
    FD_ZERO(&fds); // a new empty fd_set

    // add our fd to the set
    FD_SET(fd, &fds);

    // update the set to contain only 'ready' file descriptors
    select(fd+1, NULL, &fds, NULL, &t);

    // is our filedescriptor not in the ready set?
    if( ! FD_ISSET(fd, &fds)){
        // no its not! we will block!
        log_socket_block();
    }
}

/* * * * * * * * * * LOGGING HELPER FUNCTIONS * * * * * * * * * */

// it's important that these functions are ONLY called when you have a lock
// on the logfile. They assume logfile mutex is held (and logfile != NULL)

// log the current time
static void log_time();

// log the ip address behind this socket (IPv4 ONLY!)
static void log_addr(int socket);

// log this mastermind code
static void log_code(code_t code);

// log this mastermind hint
static void log_hint(hint_t hint);

// log this mastermind feedback
static void log_feedback(feedback_t feedback);

// log this message, or the first part of it if it is long
static void log_message(const char* message, short len);



// log the current time
static void log_time(){
    struct timeval time;

    int n = gettimeofday(&time, NULL);

    if(n == 0){

        // convert seconds to human-readable format
        time_t seconds = time.tv_sec;
        struct tm* date = localtime(&seconds);

        // http://en.cppreference.com/w/c/chrono/strftime format string rules
        char buffer[19+1];          // length of result (including null byte)
        strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", date);
        
        // log result, date and microseconds
        fprintf(logfile, "%s.%06d", buffer, (int)time.tv_usec);

    } else {
        perror("error getting system time");
        fprintf(logfile, "unable to read system time");
    }
}

// log the ip address behind this socket (IPv4 ONLY!)
static void log_addr(int socket){
    
    // use sockaddr_storage to keep address in family-agnostic manner
    struct sockaddr_storage client;
    socklen_t client_size = sizeof client;

    int n = getpeername(socket, (struct sockaddr*)&client, &client_size);
    
    if(n == 0){

        // WARNING! The following code works on the assumption that
        // the ip address behind this socket is an IPv4 addresses
        char ip[INET_ADDRSTRLEN];
        
        // returns null on error
        if(inet_ntop(AF_INET, &client, ip, INET_ADDRSTRLEN)){

            // success!
            fprintf(logfile, "%s", ip);

        } else {
            // null! failure
            perror("error presenting ip address with inet_ntop");
            fprintf(logfile, "unknown ip address");
        }
    } else {
        perror("error looking up client ip");
        fprintf(logfile, "unknown ip address");
    }
}

// log this mastermind code
static void log_code(code_t code){

    // print the code to the logfile
    for(int i = 0; i < CODE_LENGTH; i++){
        fprintf(logfile, "%c", code.c[i]);
    }
}

// log this mastermind hint
static void log_hint(hint_t hint){
    fprintf(logfile, "[%d:%d]", hint.b, hint.m);
}

// log this mastermind feedback
static void log_feedback(feedback_t feedback){

    char* message;
    if(feedback == NICETRY){
        message = "NICETRY";
    } else if (feedback == FAILURE){
        message = "FAILURE";
    } else if (feedback == CORRECT){
        message = "CORRECT";
    } else if (feedback == INVALID){
        message = "INVALID";
    } else {
        message = "UNKNOWN";
    }

    // and actually write this to the logfile
    fprintf(logfile, "%s", message);
}

// number of characters of message to log
#define MESSAGE_SNIPPET_LENGTH 30

// log this message, or the first part of it if it is long
static void log_message(const char* message, short len){
    if(len <= MESSAGE_SNIPPET_LENGTH){
        // log whole message
        fprintf(logfile, "%s", message);
    } else {
        // log first MESSAGE_SNIPPER_LENGTH characters
        fprintf(logfile, "%.*s...", MESSAGE_SNIPPET_LENGTH-3, message);
    }
}


/* * * * * * * * * * EXTERNAL LOGGING FUNCTIONS * * * * * * * * * */

// start a new log file with this filepath
// NOTE: Does not currently reset server statistics to zero
void start_log(const char* filepath){

    // (don't want two people to open a new log file at the same time!)
    // (also don't want anyone to be currently writing to a logfile!)
    lock_logfile();

    if(logfile == NULL){

        // if there's no current logfile, open one at this filepath!
        // (if there is, this will truncate it to 0 bytes)
        FILE* fp = fopen(filepath, "w");

        if(fp == NULL){
            // file open failed!
            perror("failed to open logfile");

        } else {
            // save this logfile
            logfile = fp;

            // and log an initial message
            fprintf(logfile, "(");
            log_time();
            fprintf(logfile, ") (server) starting log\n");
        }

    } else {

        fprintf(stderr, "error: a logfile is already open!\n");
    }

    unlock_logfile();

    // NOTE: to extend this to multiple logging phases, zero stats struct
    // (with a hold on the lock, ofc. potentially both at once to make sure
    // everyone is finished writing to the new stats file )
}

// log connection to new client at this socket
void log_connection(int socket){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) connecting to new client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, ")\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();


    // and update the stats!
    lock_stats();

    stats.connections++;

    unlock_stats();
}

// log connection to new client behind this socket
void log_disconnection(int socket){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) lost connection to client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, ")\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();

    // and update the stats!
    lock_stats();

    stats.disconnections++;
    
    unlock_stats();
}

// log decision on new code
void log_make_code(code_t code){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) choosing new code for client: ");
        log_code(code);
        fprintf(logfile, "\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();
}

// log a guess received from a client behind this socket
void log_recv_code(int socket, code_t code){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) receiving new code from client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, "):");
        log_code(code);
        fprintf(logfile, "\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();
}

// log feedback sent to a client behind this socket
void log_send_feedback(int socket, feedback_t feedback){
    
    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) sending feedback to client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, "):");

        log_feedback(feedback);     
        fprintf(logfile, "\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();

    // also, update the stats!

    if(feedback == CORRECT || feedback == FAILURE){
        
        lock_stats();

        if(feedback == CORRECT){
            stats.winning_clients++;
        } else {
            stats.losing_clients++;
        }

        unlock_stats();
    }
}

// log a hint sent to a client behind this socket
void log_send_hint(int socket, hint_t hint){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) sending hint to client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, "):");

        log_hint(hint);
        fprintf(logfile, "\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();
}

// log a code sent to a client behind this socket
void log_send_code(int socket, code_t code){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) sending secret code to client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, "):");

        log_code(code);
        fprintf(logfile, "\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();
}

// log a message sent to a client behind this socket
void log_send_message(int socket, const char* message){

    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ")");

        // log the message
        fprintf(logfile, " (server) sending message to client ");

        fprintf(logfile, "(sid %d)(ip ", socket);
        log_addr(socket);
        fprintf(logfile, "):");

        log_message(message, strlen(message));
        fprintf(logfile, "\n");

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();
}

// finalise log file and clean up logging resources
void close_log(){

    // wait for everyone to finish using the logfile
    lock_logfile();

    if(logfile != NULL){        

        // log the time
        fprintf(logfile, "\n(");
        log_time();
        fprintf(logfile, ")");

        fprintf(logfile, " (server) closing logfile!\nsummary:\n");

        // we ALSO need a lock on the stats mutex so that everyone is finished 
        // updating it before we log these stats

        lock_stats();

        fprintf(logfile, " connections: %d\n", stats.connections);

        fprintf(logfile, " winning clients: %d", stats.winning_clients);
        fprintf(logfile, " losing clients: %d\n", stats.losing_clients);
        fprintf(logfile, " disconnecting clients: %d\n", stats.disconnections);

        fprintf(logfile, " total blocks on mutexes: %d\n", stats.mutex_blocks_log + stats.mutex_blocks_stat);
        fprintf(logfile, " total blocks on sockets: %d\n",stats.socket_blocks);
        
        unlock_stats();

        // also get rusage at this point, to add to the summary!
        struct rusage r;
        int n = getrusage(RUSAGE_SELF, &r);
        if(n < 0){ perror("rusage"); }

        // we're particularly interested in voluntary and nonvoluntary
        // context switches!

        fprintf(logfile, " total voluntary context switches:   %ld\n",
            r.ru_nvcsw);
        fprintf(logfile, " total involuntary context switches: %ld\n",
            r.ru_nivcsw);

        // what other nice information can we print for the tail end of the log?

        // turns out these don't seem to be maintained, unfortunately

        // fprintf(logfile, " total number of writes: %ld\n", r.ru_oublock);
        // fprintf(logfile, " total number of reads:  %ld\n", r.ru_inblock);


        // and close the logfile!
        fprintf(logfile, "(");
        log_time();
        fprintf(logfile, ") (server) bye!\n");

        fclose(logfile);
        logfile = NULL;

    } else {
        fprintf(stderr, "error: no logfile currently exists!\n");
    }

    unlock_logfile();
}
