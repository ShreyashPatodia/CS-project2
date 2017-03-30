#!/bin/bash

# # # # # # # # # # # # # #
# nclients.sh for spamming
# a mastermind server with
# guesses from n clients!!
# 
# 'we will break the code'
#
# matt farrugia 25/05/2016
#
# usage: sh nclients.sh n
#		n: the number of clients to run together
#

# options:
HOST="localhost"
PORT="1024"
GUESSES="guesses.txt"

# run client n times (where n is the first command line argument)
for ((i=0;i<$1;i++))
do
	./client $HOST $PORT < $GUESSES &
done

# wait for all clients to finish
wait
echo "$1 clients done!"

# sample guesses.txt:
#FACE
#CAFE
#FADE
#ACED
#DEAF
#BEEF
#BEAD
#FEED
#BADE
#CEDE
#