# o1-koehler.2
Run make to get the executables for the program. 
./slavedriver is the master program. You may use any or none of the options with required arguments.
I used a struct for the shared memory to share the necessary info across processes. 
I only used one sleep function, and for 5 seconds at that, at the end of the critical section so as to allow other processes
  to "randomly" get in out of order and to show that the variable can be incremented and written to a log at a very high rate 
  without race conditioning. 
Output has been colorified a bit. 
When the master process times out before all the slaves are done or the user sends CTRL-C, master tries to gracefully kill the slave processes by letting them finish
  their work in their current loop. If they are not finished in time, the slaves will terminate with SIGTERM/SIGKILL.

