# COMP30023 Computer Systems 2016 --- Project 1

From the README submitted with my 2016 Computer Systems project (see specification.pdf for the task):


> Hi there!
>
> Once again, my submission for this project has become rather expansive! I've tried to keep things modular and extensibe and also safe from potential issues such as buffer overflow. The results this time is not quite as many `.c` files as last time, but I feel like it still requires some explanation.
>
> So, here's a breif guide to the core components.
> 
> Matt

CLIENT SIDE
-----------

#### client.c

contains all the functionality relevant to processing command line arguments
and connecting to a single server, at which point it hands over control to
the codebreaker module

#### codebreaker.c

a module responsible for playing a game of mastermind against a 'codemaker',
which is just a socket file descriptor from the point of view of this module

this codebreaker is user-driven, it gets its guesses from stdin before
sending them to its codemaker for feedback


SERVER SIDE
-----------

#### server.c

contains all the functionality realted to processing command line arguments
and waiting to `accept()` connection requests from incoming codebreakers, at
which point, it will create a new thread and hand over control of that 
thread to the codemaker module

also creates and maintains a log file through the logging module

#### codemaker.c

a module responsible for playing a game of mastermind against a
'codebreaker', which is just a socket file descriptor from the point of view
of this module

this codemaker also makes contributions to a log file through the logging
module

#### log.c

contains all the functionality required for starting and maintaining a log
of server behaviour, ensuring thread-safe access to a log file by hiding the
log file behind a set of external functionality for making contributions,
(described in `log.h`) and internally managing concurrency using mutexes

*(kind of like how the linux kernel provides access to privilleged function-
ality through system calls! except without the added security of a mode bit
or permissions checks)*

also stores statistics while the server is in use and writes them to the
end of the file when the server performs a shutdown

MASTERMIND NETWORK PROTOCOL
---------------------------

#### mastermind.c

provides functionality for communication of codes, feedback, hints and also
arbitrary string messages between codemakers and codebreakers, through a
fixed set of functions and constants found in `mastermind.h`

the idea here is that the codemaker and codebreaker don't have to worry
about the details of how their conversation is made possible over the
network. instead, they are able to work at a *higher level of abstraction*,
while this module takes care of reliably sending the messages through the
socket

OTHER
-----

#### Makefile

contains top level target (`both`) for compiling both a `server` and 
`client` binary executable, as well as individual targets (`client` and 
`server`, as you might expect)

#### README

you are here; overview of the architecture of my solution

#### ip.txt (REMOVED)

as specified, contains the IP address of my NeCTAR Research Cloud VM

#### report.txt

contains 500 hand-picked words describing the behaviour of my solution on
different platforms as required, specifically in terms of the number of
socket blocks, mutex blocks tracked during server lifetime in comparison
to the number of voluntary and involuntary context switches from `rusage`.


THREADS + SIGNALS
-----------------

A special note to explain an extension I made to my server just to be pedantic:

A though occured to me while I was writing the signal handling functionality 
for closing my server and writing the final statistics to the end of the logfile

> 'which thread handles these asynchronous signals?'
>
> 'what would happen if they were currently holding the logfile mutex when they left to enter the cleanup routine?'

The answer is, if they were holding the mutex, they would have to wait for themselves to finish writing to the logfile before they could obtain the logfile mutex in order to write the final statistics to the logfile and get back to finishing their work writing to the logfile so that they could unlock the mutex!

In other words, a potential deadlock, if the servicing thread is holding a mutex.

Unacceptable! Even if our server will only ever be terminated with no clients
connected and therefore no logging going on.

---

All promising paths seem to involve using a special 'cleanup thread' who does
no logging themselves and can try to write the final statistics and then exit.
The signal servicing the interrupt/signal could start this thread and then get
back to their own business, for example.

I went for a slightly different solution that ended up teaching me a lot about
mixing signal handling and pthreads. I had a lot of fun trawling through man
pages and stackoverflow trying to figure out how to reliably do this. I ended
up using the following strategy:

* at startup, server creates a dedicated signal handling thread for responding
to signals
* server then guarantees that this thread will be the one to receive signals
as it changes its sigmask (and the sigmask of all its subsequently created
codemaker client-handling threads) to block the terminating signals using
`sigset_t`'s and `pthread_sigmask()`
* the cleanup thread will perform no logging and will therefore never have a
mutex
* in fact, it does nothing much at all, except for installing the signal
handling routines using `sigaction()` (apparently a lot better than plain
old `signal()`) and then blocking itself until a signal arrives (so as
not to consume CPU time) using `sigsuspend()`
* now, when a signal arrives, the cleanup thread responds to it using the
signal handler: waiting for a lock on the logfile to close it, then exiting

Fun!

> That's all, folks!
> ------------------
>
> Anyway, I hope you have as much fun marking my solution as I have had writing
it (although, having experience with marking Computing assignments, I know
that's *probably not going to be the case).

> Matt