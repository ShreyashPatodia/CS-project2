/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                              Codemaker Module                             *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 23/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// play a game of mastermind as the codemaker against a codebreaker through
// socket 'breaker', with secret code 'code_str'
//
// if the 'code_str' is NULL, or invalid (in its length or its symbols)
// then a random code will be generated and used (NOTE: srand() will be called)
//
// assumes a logfile has been opened, and will log actions to this logfile
//
// returns on network error or finished game
void play_codemaker(int breaker, char* code_str);