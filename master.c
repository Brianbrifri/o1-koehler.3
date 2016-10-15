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
#include <errno.h>
#include <time.h>

#include "struct.h"

void interruptHandler(int);
void processDestroyer(void);
void sendMessage(int, int);
void getMessage(int, int);
int detachAndRemove(int, long long*);
void printHelpMessage(void);
void printShortHelpMessage(void);

pid_t myPid, childPid;
int slaveQueueId;
int masterQueueId;
int nextProcessToSend = 1;
int messageReceived = 0;

const int TOTAL_SLAVES = 100;
const int MAXSLAVE = 20;
const long long INCREMENTER = 10;

int main (int argc, char **argv)
{
  int shmid;
  long long *ossTimer = 0;
  key_t timerKey = 148364;
  key_t masterKey = 128464;
  key_t slaveKey = 120314;
  int hflag = 0;
  int nonOptArgFlag = 0;
  int index;
  int sValue = 5;
  int tValue = 5;
  FILE *file;
  char *filename = "test.out";
  char *defaultFileName = "test.out";
  char *programName = argv[0];
  char *option = NULL;
  char *short_options = "hs:l:t:";
  int c;
  int status;


  struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {0,     0,            0,  0},
    {}
  };
  
  //process arguments
  opterr = 0;
  while ((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1)
    switch (c) {
      case 'h':
        hflag = 1;
        break;
      case 's':
        sValue = atoi(optarg);
        if(sValue > MAXSLAVE) {
          sValue = 20;
          fprintf(stderr, "No more than 20 slave processes allowed at a time. Reverting to 20.\n");
        }
        break;
      case 'l':
        filename = optarg;
        break;
      case 't':
        tValue = atoi(optarg);  
        break;
      case '?':
        if (optopt == 's') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          sValue = 5;
        }
        else if (optopt == 'l') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          filename = defaultFileName;
        }
        else if (optopt == 't') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          tValue = 20;
        }
        else if (isprint (optopt)) {
          fprintf(stderr, "Unknown option -%c. Terminating.\n", optopt);
          return -1;
        }
        else {
          printShortHelpMessage();
          return 0; 
        }
      }

  
  //print out all non-option arguments
  for (index = optind; index < argc; index++) {
    fprintf(stderr, "Non-option argument %s\n", argv[index]);
    nonOptArgFlag = 1;
  }

  //if above printed out, print short help message
  //and return from process
  if(nonOptArgFlag) {
    printShortHelpMessage();
    return 0;
  }

  //if help flag was activated, print help message
  //then return from process
  if(hflag) {
    printHelpMessage();
    return 0;
  }

  //****START PROCESS MANAGEMENT****//
  
  //Initialize the alarm and CTRL-C handler
  signal(SIGALRM, interruptHandler);
  signal(SIGINT, interruptHandler);
  
  //set the alarm to tValue seconds
  alarm(tValue);

  //Try to get the shared mem id from the key with a size of the struct
  //create it with all perms
  if((shmid = shmget(timerKey, sizeof(long long), IPC_CREAT | 0777)) == -1) {
    perror("Bad shmget allocation");
    exit(-1);
  }

  //Try to attach the struct pointer to shared memory
  if((ossTimer = (long long *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("Master could not attach shared mem");
    exit(-1);
  }

  if((slaveQueueId = msgget(slaveKey, IPC_CREAT | 0777)) == -1) {
    perror("Master msgget for slave queue");
    exit(-1);
  }

  if((masterQueueId = msgget(masterKey, IPC_CREAT | 0777)) == -1) {
    perror("Master msgget for master queue");
    exit(-1);
  }


  //Open file and mark the beginning of the new log
  file = fopen(filename, "a");
  if(!file) {
    perror("Error opening file");
    exit(-1);
  }
  
  fprintf(file,"***** BEGIN LOG *****\n");
 
  //Malloc some space for the args going into the slaves
  char *mArg = malloc(20);
  char *nArg = malloc(20);
  char *tArg = malloc(20);

  //Initialize some shared memory variables

  int j;
  
  //Fork sValue processes
  for(j = 0; j < sValue; j++) {
  
    //exit on bad fork
    if((childPid = fork()) < 0) {
      perror("Fork Failure");
      exit(1);
    }

    //If good fork, continue to call exec with all the necessary args
    if(childPid == 0) {
      childPid = getpid();
      pid_t gpid = getpgrp();
      sprintf(mArg, "%d", shmid);
      sprintf(nArg, "%d", j + 1);
      sprintf(tArg, "%d", tValue);
      char *slaveOptions[] = {"./slaverunner", "-l", filename, "-m", mArg, "-n", nArg, "-t", tArg, (char *)0};
      execv("./slaverunner", slaveOptions);
      fprintf(stderr, "    Should only print this in error\n");
    }
  }

  free(mArg);
  free(nArg);
  free(tArg);

  sendMessage(slaveQueueId, nextProcessToSend);


  //while(1) {
  while(messageReceived < sValue) {
    *ossTimer = *ossTimer + INCREMENTER;
    getMessage(masterQueueId, 3);
    //printf("Master ossTimer: %i\n", *ossTimer);
  }

  //Wait for sValue number of processes to finish
//  for(j = 1; j <= sValue; j++) {
//    childPid = wait(&status);
//    fprintf(stderr, "Master: Child %d has died....\n", childPid);
//    fprintf(stderr, "%s*****Master: %s%d%s/%d children are dead*****%s\n",YLW, RED, j, YLW, sValue, NRM);
//  }
 
  //Detach and remove the shared memory after all child process have died
  if(detachAndRemove(shmid, ossTimer) == -1) {
    perror("Failed to destroy shared memory segment");
    return -1;
  }

  msgctl(slaveQueueId, IPC_RMID, NULL);
  msgctl(masterQueueId, IPC_RMID, NULL);

  return 0;
}

//Interrupt handler function that calls the process destroyer
//Ignore SIGQUIT and SIGINT signal, not SIGALRM, so that
//I can handle those two how I want
void interruptHandler(int SIG){
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  if(SIG == SIGINT) {
    fprintf(stderr, "\n%sCTRL-C received. Calling shutdown functions.%s\n", RED, NRM);
  }

  if(SIG == SIGALRM) {
    fprintf(stderr, "%sMaster has timed out. Initiating shutdown sequence.%s\n", RED, NRM);
  }

  kill(getpid(), SIGKILL);
  //processDestroyer();
}

//Process destroyer. 
//kill calls SIGQUIT on the groupid to kill the children but
//not the parent
void processDestroyer() {
  kill(-getpgrp(), SIGQUIT);
}

void sendMessage(int qid, int msgtype) {
  struct msgbuf msg;

  msg.mType = msgtype;
  sprintf(msg.mText, "Time to enter CS\n");
  
  if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("Master msgsnd error");
  }

}

void getMessage(int qid, int msgtype) {
  struct msgbuf msg;

  if(msgrcv(qid, (void *) &msg, sizeof(msg.mText), msgtype, MSG_NOERROR | IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("Slave msgrcv");
    }
  }
  else {
    printf("Message received: %s", msg.mText);
    messageReceived++;
    sendMessage(slaveQueueId, ++nextProcessToSend);
  }
}


//Detach and remove function
int detachAndRemove(int shmid, long long *shmaddr) {
  printf("Master: Detach and Remove Shared Memory\n");
  int error = 0;
  if(shmdt(shmaddr) == -1) {
    error = errno;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    error = errno;
  }
  if(!error) {
    return 0;
  }

  return -1;
}

//Long help message
void printHelpMessage(void) {
    printf("\nThank you for using the help menu!\n");
    printf("The following is a helpful guide to enable you to use this\n");
    printf("slavedriver program to the best of your ability!\n\n");
    printf("-h, --help: Prints this help message.\n");
    printf("-s: Allows you to set the number of slave process waiting to run.\n");
    printf("\tThe default value is 5. The max is 20.\n");
    printf("-l: Allows you to set the filename for the logger so the aliens can see how bad you mess up.\n");
    printf("\tThe default value is test.out.\n");
    printf("-t: Allows you set the wait time for the master process until it kills the slaves.\n");
    printf("\tThe default value is 20.\n");
}

//short help message
void printShortHelpMessage(void) {
  printf("\nAcceptable options are:\n");
  printf("[-h], [--help], [-l][required_arg], [-s][required_arg], [-t][required_arg]\n\n");
}


