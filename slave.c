#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#include "struct.h"

void alarmHandler(int);
void sigquitHandler(int);
void zombieKiller(int);
volatile sig_atomic_t sigNotReceived = 1;
pid_t myPid;
int processNumber = 0;
const int QUIT_TIMEOUT = 10;

int main (int argc, char **argv) {
  srand(time(0));
  int numOfWrites = 3;
  int timeoutValue = 30;
  int shmid = 0;
  data *sharedStates;
  int *sharedInt;
  char *fileName;
  char *defaultFileName = "test.out";
  char *option = NULL;
  char *short_options = "l:m:n:t:";
  FILE *file;
  int c;
  myPid = getpid();

  //get options from parent process
  opterr = 0;
  while((c = getopt(argc, argv, short_options)) != -1) 
    switch (c) {
      case 'l':
        fileName = optarg;
        break;
      case 'm':
        shmid = atoi(optarg);
        break;
      case 'n':
        processNumber = atoi(optarg);
        break;
      case 't':
        timeoutValue = atoi(optarg) + 10;
        break;
      case '?':
        fprintf(stderr, "    Arguments were not passed correctly to slave %d. Terminating.", myPid);
        exit(-1);
    }

  //Try to attach to shared memory
  if((sharedStates = (data *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("    Could not attach shared mem");
    exit(1);
  }
  
  //Ignore SIGINT so that it can be handled below
  signal(SIGINT, SIG_IGN);

  //Set the sigquitHandler for the SIGQUIT signal
  signal(SIGQUIT, sigquitHandler);

  //Set the alarm handler
  signal(SIGALRM, zombieKiller);
  
  //Set an alarm to 10 more seconds than the parent process
  //so that the child will be killed if parents gets killed
  //and child becomes slave of init
  alarm(timeoutValue);

  int i = 0;
  int j;
  int random;

  //While loop to write numOfWrites times as long as the quit signal has not been received
  while(i < numOfWrites && sigNotReceived) {
    do {

      //Raise my flag
      sharedStates->flag[processNumber] = want_in;
      //Set local variable
      j = sharedStates->turn;

      //Wait until it's my turn
      //This while loop short circuits on the j process if it is not idle, otherwise it goes to then next process then 
      //waits on it
      while(j != processNumber) {
        j = (sharedStates->flag[j] != idle) ? sharedStates->turn : (j + 1) % sharedStates->totalProcesses;
      }


      //Declare intentions to enter critical section
      sharedStates->flag[processNumber] = in_cs;
      //Check that no one else is in the critical section
      for(j = 0; j < sharedStates->totalProcesses; j++) {
        if((j != processNumber) && (sharedStates->flag[j] == in_cs)) {
          break;
        }
      }

    }while ((j < sharedStates->totalProcesses) || (sharedStates->turn != processNumber && sharedStates->flag[sharedStates->turn] != idle));

    //Increment shared variable
    sharedStates->sharedInt++;
    fprintf(stderr,"    Slave %d about to enter critical section...\n", processNumber + 1);

    //Assign turn to self and enter critical section
    sharedStates->turn = processNumber;

    random = rand() % 3;
//    sleep(random);


    file = fopen(fileName, "a");
    if(!file) {
      perror("    Error opening file");
      exit(-1);
    }

    time_t times = time(NULL);
    
    fprintf(file,"    File modified by process number %d at time %lu with shared number %d\n", processNumber + 1, times, sharedStates->sharedInt);

    if(fclose(file)) {
      perror("    Error closing file");
    }

    fprintf(stderr,"    Slave %d exiting critical section...\n", processNumber + 1);

    //Exit section
    j = (sharedStates->turn + 1) % sharedStates->totalProcesses;

    //find the next process that is not idle
    while(sharedStates->flag[j] == idle) {
      j = (j + 1) % sharedStates->totalProcesses;
    }

    //Assign turn to next waiting process and change own flag to idle
    sharedStates->turn = j;
    sharedStates->flag[processNumber] = idle;
    
    //Do a random sleep here so that the process idles in "idle mode"
    //so that other process can randomly take next turn instead of the 
    //next numbered process
    random = rand() % 5;
    sleep(random);

    i++;
  }
  
  //Do some final printing based on if all iterations were accomplished
  if(i == numOfWrites) {
    fprintf(stderr, "    Slave %d %sCOMPLETED WORK%s\n", processNumber + 1, GREEN, NRM);
  }
  else {
    fprintf(stderr, "    Slave %d did %sNOT %scomplete work\n", processNumber + 1, RED, NRM);
  }

  if(shmdt(sharedStates) == -1) {
    perror("    Slave could not detach shared memory");
  }

  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
  
}

//This handles SIGQUIT being sent from parent process
//It sets the volatile int to 0 so that the while loop will exit. 
void sigquitHandler(int sig) {
  printf("    Slave %d has received signal %s (%d)\n", processNumber, strsignal(sig), sig);
  sigNotReceived = 0;
  //The slaves have at most 10 more seconds to exit gracefully or they will be SIGTERM'd
  alarm(QUIT_TIMEOUT);
}

//function to kill itself if the alarm goes off,
//signaling that the parent could not kill it off
void zombieKiller(int sig) {
  printf("    %sSlave %d is killing itself due to slave timeout override%s\n",MAG, processNumber, NRM);
  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
}
