#ifndef STRUCT_H
#define STRUCT_H
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YLW "\x1b[33m"
#define BLU "\x1b[34m"
#define MAG "\x1b[35m"
#define NRM "\x1b[0m"
enum state { idle, want_in, in_cs};

typedef struct data_struct {
  int sharedInt;
  int turn;
  int totalProcesses;
  enum state flag[];
} data;

#endif
