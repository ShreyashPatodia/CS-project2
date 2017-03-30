/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                              Codemaker Module                             *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 23/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "codemaker.h"
#include "mastermind.h" // agreed-upon game-playing protocol (types + functions)

#include "log.h"

// the maximum number of guesses (valid or invalid) for each codebreaker
#define GUESS_LIMIT 10

// the string to send the codebreakers at the start of each game
#define WELCOME_MSG "Welcome to mastermind, codebreaker.\n"\
                    "Try to guess the secret code before you "\
                    "run out of guesses!\nValid symbols are "\
                    SYMBOLS\
                    "\nYou have 10 guesses (including invalid "\
                    "guesses).\nGood luck!"

// MACRO to check if a socket i/o return value is good,
// or bad (in which case, return from the calling function)
#define check_return(n)\
    if((n) < 0){\
        perror("network error");\
        return;\
    } else if((n) == 0){\
        fprintf(stderr, "network error: codebreaker disconnected!\n");\
        log_disconnection(breaker);\
        return;\
    }\

/* * * * * * * * * * HELPER FUNCTION PROTOTYPES * * * * * * * * * */

// helper function for parsing a code string into a mastermind code struct
// if the code is invalid (length or symbols) or NULL then a random code
// will be generated and returned for us
code_t read_code(char* code_str);

// helper function to validate a mastermind code's symbols
// returns 1 if valid, 0 if invalid
int validate(code_t code);

// helper function to evaluate a valid guess against a secret code
hint_t evaluate(code_t guess, code_t secret);

/* * * * * * * * * * CODEMAKER ENTRY POINT * * * * * * * * * */

// play a game of mastermind as the codemaker against a codebreaker through
// socket 'breaker', with secret code 'code_str'
//
// if the 'code_str' is NULL, or invalid (in its length or its symbols)
// then a random code will be generated and used (NOTE: srand() will be called)
//
// assumes a logfile has been opened, and will log actions to this logfile
//
// returns on network error or finished game
void play_codemaker(int breaker, char* code_str){
    
    int n; // for catching return values of all the network functions we call

    // first, decide on a secret code
    code_t secret = read_code(code_str);
    log_make_code(secret);


    // next, send the client a welcome message!
    log_block_write_test(breaker);
    n = send_string(breaker, WELCOME_MSG);
    check_return(n);
    log_send_message(breaker, WELCOME_MSG);


    // now, we're reading to respond to guesses from the codebreaker!

    int remaining_guesses = GUESS_LIMIT;
    while(remaining_guesses > 0){
        
        // receive a code guess from the code breaker
        code_t guess;

        log_block_read_test(breaker);
        n = receive_code(breaker, &guess);
        check_return(n);
        log_recv_code(breaker, guess);

        // determine what to do with it!
        if( !validate(guess) ){
            // this guess contains an invalid symbol!

            remaining_guesses -= 1; // have we used up one more guess? 'YES'
            
            if(remaining_guesses > 0){
                // still have guesses left!

                // send feedback
                log_block_write_test(breaker);
                n = send_feedback(breaker, INVALID);
                check_return(n);
                log_send_feedback(breaker, INVALID);

                // send a follow up message to describe the problem
                log_block_write_test(breaker);
                n = send_string(breaker, "valid symbols are "
                    SYMBOLS);
                check_return(n);
                log_send_message(breaker, "valid symbols are "
                    SYMBOLS);

            } else {
                // out of guesses!

                // send feedback
                log_block_write_test(breaker);
                n = send_feedback(breaker, FAILURE);
                check_return(n);
                log_send_feedback(breaker, FAILURE);

                // send hint
                hint_t hint = {0, 0}; // invalid symbol on the last guess so
                                      // i guess it doesn't matter what hint
                                      // we send
                log_block_write_test(breaker);
                n = send_hint(breaker, hint);
                check_return(n);
                log_send_hint(breaker, hint);

                // and send code
                log_block_write_test(breaker);
                n = send_code(breaker, secret);
                check_return(n);
                log_send_code(breaker, secret);
            }

            // skip to next guess
            continue;
        }

        // now that we're dealing with a valid guess,
        // let's evaluate it against the secret code
        hint_t hint = evaluate(guess, secret);

        if(hint.b == CODE_LENGTH){

            // all code symbols correct! codebreaker guessed the codeword!
            
            // send feedback
            log_block_write_test(breaker);
            n = send_feedback(breaker, CORRECT);
            check_return(n);
            log_send_feedback(breaker, CORRECT);

            // this should hit us stright outa the game loop
            remaining_guesses = 0; 

        } else {

            // we've used up one more guess
            remaining_guesses -= 1;

            if(remaining_guesses > 0){
                // still have guesses left!

                // send feedback
                log_block_write_test(breaker);
                n = send_feedback(breaker, NICETRY);
                check_return(n);
                log_send_feedback(breaker, NICETRY);

                // send hint
                log_block_write_test(breaker);
                n = send_hint(breaker, hint);
                check_return(n);
                log_send_hint(breaker, hint);

            } else {
                // out of guesses!

                // send feedback
                log_block_write_test(breaker);
                n = send_feedback(breaker, FAILURE);
                check_return(n);
                log_send_feedback(breaker, FAILURE);

                // send hint
                log_block_write_test(breaker);
                n = send_hint(breaker, hint);
                check_return(n);
                log_send_hint(breaker, hint);

                // and send code
                log_block_write_test(breaker);
                n = send_code(breaker, secret);
                check_return(n);
                log_send_code(breaker, secret);
            }
        }
    }
}

/* * * * * * * * * * HELPER FUNCTION CODE * * * * * * * * * */

// helper function for parsing a code string into a mastermind code struct
// if the code is invalid (length or symbols) or NULL then a random code
// will be generated and returned for us
code_t read_code(char* code_str){
    if(code_str){

        // make sure the code is the right length
        int len = strlen(code_str);
        if(len == CODE_LENGTH){
            
            // create new code, copying string's chars
            code_t code;

            for(int i = 0; i < CODE_LENGTH; i++){
                code.c[i] = code_str[i];
            }

            return code;
        }

        // if not, print an error
        fprintf(stderr, "invalid code provided (length %d, should be %d)\n",
            len, CODE_LENGTH);
        fprintf(stderr, "defaulting to random code generation\n");

        // fall out of this branch to random generation
    }

    // if we get here, there was no (valid) code provided
    // randomly generate a code

    srand(clock());

    code_t code;
    int n_symbols = strlen(SYMBOLS);

    for(int i = 0; i < CODE_LENGTH; i++){
        // pick a random valid symbol to be the ith code symbol
        code.c[i] = SYMBOLS[rand() % n_symbols];
    }

    return code;
}

// helper function to validate a mastermind code's symbols
// returns 1 if valid, 0 if invalid
int validate(code_t code){

    int n = strlen(SYMBOLS);

    // for each symbol in the code
    for(int i = 0; i < CODE_LENGTH; i++){

        int valid = 0; // is this symbol valid?
        for(int j = 0; j < n; j++){
            if(SYMBOLS[j] == code.c[i]){
                valid = 1;
                break;
            }
        }

        if(!valid){
            // it's not valid!
            return 0;
        }
    }

    // if we make it to here, all symbols must be valid
    return 1;
}

// helper function to evaluate a valid guess against a secret code
hint_t evaluate(code_t guess, code_t secret){

    hint_t hint = {0, 0};

    int mark[CODE_LENGTH];

    // initialse mark array while also counting
    // correct symbols (correct symbol + position)
    for(int i = 0; i < CODE_LENGTH; i++){

        if(guess.c[i] == secret.c[i]){
            // i'th symbol is correct!

            hint.b += 1;
            mark[i] = 2;
        } else {

            // initialise array anyway
            mark[i] = 0;
        }
    }

    // count 'missed' symbols (correct symbols, incorrect position)

    // for each unmarked guess symbol
    for(int i = 0; i < CODE_LENGTH; i++){
        if(mark[i] < 2){
        
            // for each unmarked secret code symbol
            for(int j = 0; j < CODE_LENGTH; j++){
                if(mark[j] == 0 && i != j){

                    if(secret.c[j] == guess.c[i]){
                        // we found it! right symbol wrong spot!
                        
                        hint.m += 1;
                        mark[j] = 1;
                        break;
                    }
                }
            }
        }
    }

    return hint;
}
