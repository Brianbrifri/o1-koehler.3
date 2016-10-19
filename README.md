# o1-koehler.3
"make" build the program.
./oss runs the program. 
Options to change runtime can be found by add -h or --help to ./oss
The master sets up shared memory, masterqueue and slavequeue.
Then it spawns x number of slaves (default 5), sends a message to the slave queue to kick them off
then starts updating the timer. The slaves will only break out of CS and into the rest of the code when its time is up
The master in constantly checking its queue to see if a slave sent it a message. If not, it continues due to IPC_NOWAIT 
otherwise it processes the message and and prints to file then sends a message back to the slave queue so that they will
start trying to enter the CS again.
The master flips the sigNotReceived bit to false if it no longer wants the slaves to try to enter CS. This happens when time runs out 
or Interrupt from user received. 
The master then waits for all the children to die then cleans up the queues and shared memory
