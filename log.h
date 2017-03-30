/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *               COMP30023 Computer Systems - Semester 1 2016                *
 *           Assignment 2 - Socket Programming and Synchonisation            *
 *                                                                           *
 *                               Logging Module                              *
 *                                                                           *
 *                  Submission by: Matt Farrugia <farrugiam>                 *
 *                  Last Modified 24/05/16 by Matt Farrugia                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "mastermind.h" // for types associated with mastermind game

// start a new log file with this filepath
 // NOTE: Does not currently reset server statistics to zero
void start_log(const char* filepath);

// log connection to new client behind this socket
void log_connection(int socket);

// log blocking operation on this socket
void log_block_read_test(int fd);
void log_block_write_test(int fd);

// log disconnection from client behind this socket
void log_disconnection(int socket);

// log a new secret code
void log_make_code(code_t code);

// log a guess received from a client behind this socket
void log_recv_code(int socket, code_t code);

// log feedback sent to a client behind this socket
void log_send_feedback(int socket, feedback_t feedback);

// log a hint sent to a client behind this socket
void log_send_hint(int socket, hint_t hint);

// log a code sent to a client behind this socket
void log_send_code(int socket, code_t code);

// log a message sent to a client behind this socket
void log_send_message(int socket, const char* message);

// finalise log file and clean up logging resources
void close_log();