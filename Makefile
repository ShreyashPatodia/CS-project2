#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
#*               COMP30023 Computer Systems - Semester 1 2016                *
#*           Assignment 2 - Socket Programming and Synchonisation            *
#*                                                                           *
#*                                 Makefile                                  *
#*                                                                           *
#*                  Submission by: Matt Farrugia <farrugiam>                 *
#*                  Last Modified 25/05/16 by Matt Farrugia                  *
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

CC		= gcc
LDFLAGS = -Wall -pthread
CFLAGS 	= -Wall -std=gnu99 -MMD -fshort-enums 

CEXE = 	client
CSRC =	client.c codebreaker.c mastermind.c
COBJ =	client.o codebreaker.o mastermind.o

SEXE = 	server
SSRC =	server.c codemaker.c mastermind.c log.c
SOBJ =	server.o codemaker.o mastermind.o log.o

## top level target is BOTH client make and server make
both:	client server

## cleanly: make top level target, and then clean files immediately
cleanly: both clean

# target to copy files over to both servers
scp:
		scp -i /Users/Matt/dev/uni/cs/a2/k.pem *.c *.h Makefile ubuntu@115.146.85.167:cs/a2
		scp *.c *.h Makefile farrugiam@digitalis.eng.unimelb.edu.au:cs/project2
# enter password now...

client: $(COBJ)
		$(CC) $(LDFLAGS) -o $(CEXE) $(COBJ)
		rm *.d

server: $(SOBJ)
		$(CC) $(LDFLAGS) -o $(SEXE) $(SOBJ)
		rm *.d

## clean: removes object files and dependency files
clean:
		rm *.o

## rmexec: removes executable files
rmexe:
		rm $(SEXE) $(CEXE)

# clobber: clean and remove exes
clobber: clean rmexe

## dependencies
-include $(DEP)