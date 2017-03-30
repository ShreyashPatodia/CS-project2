/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                     Mastermind Network Protocol Module                    *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 20/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#pragma once // ensure types are only defined once

// an enum representing the type of message being sent to a codebreaker
// NOTE: compile with -fshort-enums to ensure sizeof (feedback_t) == 1
typedef enum {
    NICETRY = 0,    // response to a valid guess, followed by a hint_t struct
    INVALID = 1,    // response to an invalid guess, followed by an explanatory
                    // message
    CORRECT = 2,    // response to a correct guess, followed by success message
    FAILURE = 3,    // response to a breaker's final allowed guess, followed by
                    // a hint and then the correct secret code
} feedback_t;

// struct encapsulating evaluation of a codebreaker's guess
typedef struct {
    short b; // number of code symbols in correct position
    short m; // number of correct code symbols in wrong position
} hint_t;

// struct encapsulating a code (the correct code or a guess or whatever)
#define CODE_LENGTH 4       // fixed length of codes to be used
typedef struct {
    char c[CODE_LENGTH];    // a fixed length array of code symbols
} code_t;
#define SYMBOLS "ABCDEF"    // valid code symbols

/* * * * * * * * * * SENDING AND RECIEVAL FUNCTIONS * * * * * * * * * */

// RETURN VALUES
// in all of the following functions,
//  -   return value > 0 indicates success
//  -   return value = 0 indicates disconnection at other end of socket
//  -   return value < 0 indicates error (with errno set)

// send a string over this socket
// sends strings with length (including terminating byte) up to max short
int send_string(int socket, char* buffer);

// send a mastermind feedback struct over this socket
int send_feedback(int socket, feedback_t data);

// send a mastermind code struct over this socket
int send_code(int socket, code_t code);

// send a mastermind hint struct over this socket
int send_hint(int socket, hint_t hint);

// recv a string over this socket
// mallocs exactly enough space for the string (including terminating byte)
// and places the resulting pointer in *buffer
// to avoid memory leaks, please free(*buffer) once you are finished
// with the string you have recieved
int receive_string(int socket, char** buffer);

// recv a mastermind feedback struct over this socket,
// the result will be placed in *data
// upon network errors, *data is unchanged
int receive_feedback(int socket, feedback_t* data);

// recv a mastermind code struct over this socket,
// the result will be placed in *code
// upon network errors, *code is unchanged
int receive_code(int socket, code_t* code);

// recv a mastermind hint struct over this socket,
// the result will be placed in *hint
// upon network errors, *hint is unchanged
int receive_hint(int socket, hint_t* hint);
