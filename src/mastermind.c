/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                     Mastermind Network Protocol Module                    *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 20/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mastermind.h"

/* * * * * * * * * * RECEIVAL HELPER FUNCTIONS * * * * * * * * * */

// recv all length bytes over socket and place them in buffer
int recv_all(int socket, char* buffer, short length);

// recv a short over socket and place it in *data
int receive_short(int socket, short* data);

// recv all length bytes over socket and place them in buffer
int recv_all(int socket, char* buffer, short length){

    char* cursor = buffer;
    short bytes_left = length;

    while(bytes_left > 0){
        int n = recv(socket, cursor, bytes_left, 0);

        if(n <= 0){
            // error or disconnect!
            return n;
        }

        // advance cursor
        cursor += n;
        // decrement remaining byte count
        bytes_left -= n;
    }

    // success!
    return length;
}

// recv a short over socket and place it in *data
int receive_short(int socket, short* data){

    short buffer;

    int n = recv_all(socket, (char*)&buffer, sizeof(short));

    if(n <= 0){
        // error or disconnect
        return n;
    }

    // all good!

    *data = ntohs(buffer);

    return n;
}

/* * * * * * * * * * RECEIVAL FUNCTIONS * * * * * * * * * */

// recv a string over this socket
// mallocs exactly enough space for the string (including terminating byte)
// and places the resulting pointer in *buffer
// to avoid memory leaks, please free(*buffer) once you are finished
// with the string you have recieved
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int receive_string(int socket, char** buffer){
    
    // string messages are sent character by character, preceeded by their
    // length (+1 for the null byte)

    // get the length
    short length;
    int n = receive_short(socket, &length);
    
    if(n <= 0){
        // something has gone wrong!
        return n;
    }

    // create space for that many characters
    *buffer = malloc(length * sizeof **buffer);
    
    // and read into this space
    return recv_all(socket, *buffer, length);
}

// recv a mastermind feedback struct over this socket,
// the result will be placed in *data
// upon network errors, *data is unchanged
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int receive_feedback(int socket, feedback_t* data){
    // feedback is sent as a one-bit enum (assuming we compiled with 
    // -fshort-enums, else it may be 4 bits and we'd need to use recv_all
    // in order to guarantee receival all in one piece)

    feedback_t f;
    int n = recv(socket, &f, sizeof (feedback_t), 0);

    if(n <= 0){
        // error or disconnect!
        return n;
    } else if(n < sizeof(feedback_t)){
        // feedback was not delivered all in one piece!
        fprintf(stderr, "fatal: incomplete transmission\n");
        exit(EXIT_FAILURE);
    }

    // return the data
    *data = f;

    // success!
    return n;
}

// recv a mastermind code struct over this socket,
// the result will be placed in *code
// upon network errors, *code is unchanged
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int receive_code(int socket, code_t* code){
    
    // a code is sent as CODE_LENGTH consecutive bytes

    // read all bytes into a new code char array
    code_t c;
    int n = recv_all(socket, c.c, CODE_LENGTH);

    if(n <= 0){
        // error or disconnect!
        return n;
    }

    // return the data
    *code = c;

    // success!
    return n;
}

// recv a mastermind hint struct over this socket,
// the result will be placed in *hint
// upon network errors, *hint is unchanged
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int receive_hint(int socket, hint_t* hint){
    int n;

    // a hint is sent as two consecutive shorts, first 'b' then 'm'

    // let's read them into a new hint struct

    hint_t h;

    n = receive_short(socket, &(h.b));

    if(n <= 0){
        // error or disconnect!
        return n;
    }

    n = receive_short(socket, &(h.m));

    if(n <= 0){
        // error or disconnect!
        return n;
    }

    // at this point, both reads were successful

    // copy out the hint
    *hint = h;

    // success!
    return 1;
}

/* * * * * * * * * * SENDING HELPER FUNCTIONS * * * * * * * * * */

// send all length bytes in buffer over socket
int send_all(int socket, char* buffer, short length);

// send data (short) over socket socket
int send_short(int socket, short data);

// send all length bytes in buffer over socket
int send_all(int socket, char* buffer, short length){
    char* cursor = buffer;
    short bytes_left = length;

    while(bytes_left > 0){
        int n = send(socket, cursor, bytes_left, 0);

        if(n <= 0){
            // error or disconnect! return it
            return n;
        }

        // advance cursor
        cursor += n;

        // decrement remaining byte count
        bytes_left -= n;
    }

    // success!
    return length;
}

// send data (short) over socket socket
int send_short(int socket, short data){

    short buffer = htons(data);

    return send_all(socket, (char*)&buffer, sizeof(short));
}

/* * * * * * * * * * SENDING FUNCTIONS * * * * * * * * * */

// send a string over this socket
// sends strings with length (including terminating byte) up to max short
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int send_string(int socket, char* buffer){

    // send a string as its length (a short) +1 (including null byte)
    // followed by its bytes (including null byte)

    // hmm i guess we don't need to actually SEND the null byte but it's
    // a little simpler if we do, maybe optimise this out later

    // first, calculate and send the length

    short length = (short)(strlen(buffer) + 1);

    int n = send_short(socket, length);

    if(n <= 0){
        // error or disconnect!
        return n;
    }

    // then, send the string's bytes!

    return send_all(socket, buffer, length);
}

// send a mastermind feedback struct over this socket
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int send_feedback(int socket, feedback_t data){

    // send feedback as a one-bit enum
    // (compiling with gcc -fshort-enums to guarantee sizeof(feedback_t) == 1)

    int n = send(socket, &data, sizeof (feedback_t), 0);

    if(n <= 0){
        // error or disconnect!
        return n;
    } else if(n < sizeof(feedback_t)){
        // feedback was not delivered all in one piece!
        fprintf(stderr, "fatal: incomplete transmission\n");
        exit(EXIT_FAILURE);
    }

    // success!
    return n;
}

// send a mastermind code struct over this socket
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int send_code(int socket, code_t code){

    // send a code as CODE_LENGTH consecutive bytes

    // send all the bytes in this code's byte array
    return send_all(socket, code.c, CODE_LENGTH);
}

// send a mastermind hint struct over this socket
// n > 0 is returned on success
// 0 is returned for a disconnection, or
// n < 0 for a network error (with errno set)
int send_hint(int socket, hint_t hint){

    // send a hint as two consecutive shorts, first 'b', then 'm'

    int n;

    n = send_short(socket, hint.b);

    if(n <= 0){
        // error or disconnect!
        return n;
    }

    n = send_short(socket, hint.m);

    if(n <= 0){
        // error or disconnect!
        return n;
    }

    // at this point, both writes were successful!
    return 1;
}