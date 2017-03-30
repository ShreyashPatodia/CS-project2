/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                             Codebreaker Module                            *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 23/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>

#include "codebreaker.h"

#include "mastermind.h" // agreed-upon game-playing protocol (types + functions)

// MACRO to check if a socket i/o return value is good,
// or bad (in which case, return from the calling function)
#define check_return(n)\
    if((n) < 0){\
        perror("network error");\
        return;\
    } else if((n) == 0){\
        fprintf(stderr, "network error: codemaker disconnected!\n");\
        return;\
    }\

/* * * * * * * * * * HELPER FUNCTION PROTOTYPES * * * * * * * * * */

// helper function to read a valid-length guess from stdin,
// and store it in *guess
// returns 0 on success or -1 on EOF
int get_guess(code_t* guess);

// print a code's characters to stdout, not followed by a newline or anything
void print_code(code_t code);

/* * * * * * * * * * CODEBREAKER ENTRY POINT * * * * * * * * * */

// play a game of mastermind as the codebreaker against a codemaker through
// socket 'maker'
//
// returns on network error or finished game
void play_codebreaker(int maker){

    int n; // for temporarily storing return values

    // collect codebreaker welcome message

    char* msg;
    n = receive_string(maker, &msg);    // don't forget to free later!
    check_return(n);
    printf("codemaker:\n %s\n", msg);
    free(msg);                          // finished with the message

    // and then enter the game loop !

    for(;;){

        // get a 4-letter code from the user

        code_t guess;
        n = get_guess(&guess);
        if(n < 0){ return; } // make sure we have't reached EOF

        //  send this code to the codemaker!

        printf("sending code: ");
        print_code(guess);
        printf("\n");

        n = send_code(maker, guess);
        check_return(n);

        // collect feedback on this guess from the codemaker

        feedback_t feedback;
        n = receive_feedback(maker, &feedback);
        check_return(n);


        // depending on the response...

        if(feedback == NICETRY){

            // collect hint from codemaker
            hint_t hint;
            n = receive_hint(maker, &hint);
            check_return(n);

            // report feedback to user
            printf("codemaker:\n");
            printf(" %d correct symbols in a correct position\n", hint.b);
            printf(" %d correct symbols in incorrect position\n", hint.m);

        } else if(feedback == INVALID){

            // that was an invalid guess! the codemaker will send us a string
            // explaining why
            char* msg;
            n = receive_string(maker, &msg);    // don't forget to free later!
            check_return(n);

            // report message to user
            printf("an invalid guess!\ncodemaker: %s\n", msg);
            free(msg);                          // finished with the message

        } else if(feedback == CORRECT){

            // hooray we are victorious!
            printf("success! you broke the code!\n");
            break;

        } else if(feedback == FAILURE){

            // that was our last guess!

            // collect hint from codemaker
            hint_t hint;
            n = receive_hint(maker, &hint);
            check_return(n);

            // report feedback to user
            printf("codemaker:\n");
            printf(" %d correct symbols in a correct position\n", hint.b);
            printf(" %d correct symbols in incorrect position\n", hint.m);

            // collect correct code from codemaker
            code_t code;
            n = receive_code(maker, &code);
            check_return(n);

            printf("that was the last guess!\n");
            printf("the correct code was ");
            print_code(code);
            printf("\n");

            // that's the end of the game! no more guesses!
            break;

        } else {

            // feedback is not what we expected! print it and exit
            fprintf(stderr, "error: unrecognised codemaker response (%d)\n",
                feedback);
            return;
        }
    }
}

/* * * * * * * * * * HELPER FUNCTION CODE * * * * * * * * * */

// helper function to read a valid-length guess from stdin,
// and store it in *guess
// returns 0 on success or -1 on EOF
int get_guess(code_t* guess){

    // prompt user for a guess
    printf("Enter a guess: ");

    size_t line_length = CODE_LENGTH + 1;
    char* line = malloc(line_length * sizeof *line);

    int n;
    while((n = getline(&line, &line_length, stdin)) < CODE_LENGTH+1){
        if(n < 0){
            // EOF reached! return a sentinel in the first code spot
            return -1;
        } else {
            // prompt user to re-enter a guess
            printf("Please try again,\nWrite a %d-char code: ", CODE_LENGTH);
        }
    }

    for(int i = 0; i < CODE_LENGTH; i++){
        guess->c[i] = line[i];
    }

    free(line);

    return 0;
}

// print a code's characters to stdout, not followed by a newline or anything
void print_code(code_t code){
    for(int i = 0; i < CODE_LENGTH; i++){
        printf("%c", code.c[i]);
    }
}
