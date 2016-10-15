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
#include <sys/msg.h>
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
  int timeoutValue = 30;
  long long startTime;
  long long currentTime;
  long long *ossTimer;

  int shmid = 0;

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
  if((ossTimer = (long long *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("    Slave could not attach shared mem");
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
  long long duration;

  duration = 1 + rand() % 100000;
  printf("Duration gotten: %i\n", duration);
  startTime = *ossTimer;
  currentTime = *ossTimer - startTime;


  while((currentTime = (*ossTimer - startTime) / 1000) < duration) {
    //currentTime = (*ossTimer - startTime) / 1000;
    printf("Current time: %i\n", currentTime);
  }
  
  if(shmdt(ossTimer) == -1) {
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
