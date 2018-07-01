#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h> 
#include <dirent.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_ARGS_LENGTH 255
#define MAX_COMMAND_LENGTH 1024

#define NUM_FILES_CMD "num-files"
#define TOTAL_SIZE_CMD "total-size"
#define SEARCH_CHAR_CMD "search-char"
#define ERROR_CMD "Invalid command"

#define FATHER_MESSAGE_TYPE 1

/*************************/
/** PARSE SHELL COMMAND **/
/*************************/

int parseShellCommand(char* shellBuffer, char** shellArgs) {
  size_t i = 0, j = 0;
  size_t numArg = 0;

  shellArgs[0][0] = 0;
  shellArgs[1][0] = 0;
  shellArgs[2][0] = 0;
  shellArgs[3][0] = 0;
  
  while(shellBuffer[i] != 10 && shellBuffer[i] != 0 && numArg < 4) {
    j = 0;
    if(shellBuffer[i] == ' ') ++i;

    while(shellBuffer[i+j] != ' ' && shellBuffer[i+j] != 10 && shellBuffer[i+j] != 0) {
      shellArgs[numArg][j] = shellBuffer[i+j];
      ++j;
    }

    shellArgs[numArg][j] = 0;

    i += j;
    ++numArg;
  }
  
  return numArg;
}

/******************/
/** STRING COUNT **/
/******************/

int stringCount(char* string, char c, long size) {
    int count = 0;

    for(char* p = string; *p != 0; ++p) {
      if(*p == c)
        ++count;
    }

    return count;
}

/*********************/
/** MESSAGES STRUCT **/
/*********************/

typedef struct {
  long type;
  char string[MAX_COMMAND_LENGTH];
} message;
 
/************/
/** FATHER **/
/************/

int father(char* shellName, int messageQueueDescriptor, int childrenCount) {
  char** shellArgs = (char**) malloc(4*sizeof(char*));
  shellArgs[0] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));
  shellArgs[1] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));
  shellArgs[2] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));
  shellArgs[3] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));

  char* shellBuffer = (char*) malloc(MAX_COMMAND_LENGTH * sizeof(char));

  size_t numArg = 0;

  message messageBuffer;

  int stop = 0;
  while(!stop) {
    printf("%s > ", shellName);
    if(fgets(shellBuffer, MAX_COMMAND_LENGTH * sizeof(char), stdin)) {
      numArg = parseShellCommand(shellBuffer, shellArgs);
      long childId = atoi(shellArgs[1]) + 2;

      if(childId < childrenCount + 2) {
        messageBuffer.type = childId;
        strncpy(messageBuffer.string, shellBuffer, MAX_COMMAND_LENGTH);

        if(msgsnd(messageQueueDescriptor, &messageBuffer, strlen(messageBuffer.string) + 1, 0) != 0) {
          perror("error sending command:");
        } else {
          // printf("[father] sent message: %s (%lu), waiting for response...\n",  messageBuffer.string, messageBuffer.type);
          if(msgrcv(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), FATHER_MESSAGE_TYPE, 0) == -1) {
            perror("error on message response");
          } else {
            printf("Received response: %s\n", messageBuffer.string);
          }
        }
      } else {
        printf("invalid directory number (%lu)\n", childId);
      }
    }
  }
  return 0;
}

/***********/
/** CHILD **/
/***********/

int child(long id, char* dirPath, int messageQueueDescriptor, int semaphoresDescriptors) {
  char** shellArgs = (char**) malloc(4*sizeof(char*));
  shellArgs[0] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));
  shellArgs[1] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));
  shellArgs[2] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));
  shellArgs[3] = (char*) malloc(MAX_ARGS_LENGTH * sizeof(char));

  size_t numArg = 0;

  message messageBuffer;

  while(1) {
    if(msgrcv(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), id, 0) == -1) {
      messageBuffer.type = FATHER_MESSAGE_TYPE;
      strncpy(messageBuffer.string, ERROR_CMD, MAX_COMMAND_LENGTH);

      msgsnd(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), 0);
    } else {
      DIR* dir;
      if((dir = opendir(dirPath)) == NULL) {
        perror("[child] error opening a directory");
        exit(1);
      }
      
      struct stat dirStat;
      if((stat(dirPath, &dirStat)) == -1) {
        perror("[child] error analyzing a directory");
        exit(1);
      }

      numArg = parseShellCommand(messageBuffer.string, shellArgs);
      if(strcmp(shellArgs[0], NUM_FILES_CMD) == 0) { // num-files
        size_t count = 0;
        struct dirent *dirEntry;

        while((dirEntry = readdir(dir)) != NULL) {
          if(DT_REG == dirEntry->d_type)
            ++count;
        }

        messageBuffer.type = FATHER_MESSAGE_TYPE;
        sprintf(messageBuffer.string, "files = %lu", count);

        msgsnd(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), 0);
      } else if(strcmp(shellArgs[0], TOTAL_SIZE_CMD) == 0) { // total-size
        size_t totalSize = 0;

        struct dirent *dirEntry;
        struct stat *entryStat;
        char* filePath = malloc(sizeof(char) * 255);

        while((dirEntry = readdir(dir)) != NULL) {
          if(DT_REG == dirEntry->d_type) {            
            sprintf(filePath, "%s/%s", dirPath, dirEntry->d_name);
            stat(filePath, entryStat);
            totalSize += entryStat->st_size;
          }
        }

        messageBuffer.type = FATHER_MESSAGE_TYPE;
        sprintf(messageBuffer.string, "total size = %lu", totalSize);

        msgsnd(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), 0);
      } else if(strcmp(shellArgs[0], SEARCH_CHAR_CMD) == 0) { // search-char
        struct stat *fileStat;
        char* filePath = malloc(sizeof(char) * 255);
        sprintf(filePath, "%s/%s", dirPath, shellArgs[2]);
        stat(filePath, fileStat);

        int fileDescriptor;
        if(!S_ISREG(fileStat->st_mode) || (fileDescriptor = open(filePath, O_RDONLY)) == -1) {
          perror("error reading file");
        } else {
          void* mapPointer;
          if((mapPointer = mmap(NULL, fileStat->st_size, PROT_READ, MAP_PRIVATE, fileDescriptor, 0)) == (int*) -1) {
            perror("error mapping file");
          } else {
            printf("looking for occurrencies...\n");
            int occurrencies = stringCount((char*) mapPointer, shellArgs[3][0], fileStat->st_size);

            messageBuffer.type = FATHER_MESSAGE_TYPE;
            sprintf(messageBuffer.string, "occurrencies = %d", occurrencies);

            msgsnd(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), 0);
          }
        }
      } else {
        messageBuffer.type = FATHER_MESSAGE_TYPE;
        strncpy(messageBuffer.string, ERROR_CMD, MAX_COMMAND_LENGTH);

        msgsnd(messageQueueDescriptor, &messageBuffer, sizeof(messageBuffer) - sizeof(long), 0);
      }

      if((closedir(dir)) != 0) {
        perror("[child] error closing directory");
        exit(1);
      }
    }
  }

  exit(0);
}

/**********/
/** MAIN **/
/**********/

int main(int argc, char** argv) {
  if(argc < 2) {
    printf("usage: %s <directory-1> <directory-2> <...>\n", argv[0]);
    return 1;
  }

  int retcode = 0;

  size_t childNumber = 0;

  int pid = 0;

  int messageQueueDescriptor;

  // Message queue creation
  if((messageQueueDescriptor = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
    perror("error creating queue");
    return 1;
  }

  while(childNumber < argc - 1) {
    pid = fork();
    
    if(pid != 0)
      ++childNumber;
    else break;
  }

  if(pid != 0) { // father
    retcode = father(argv[0], messageQueueDescriptor, argc - 1);

    for(size_t i = 0; i < childNumber; ++i) {
      wait(NULL);
    }

    if(msgctl(messageQueueDescriptor, IPC_RMID, NULL) != 0) {
      perror("error deallocating queues");
      return 1;
    }
  } else { // child
    child(childNumber + 2, argv[childNumber+1], messageQueueDescriptor, argc - 1);
  }

  exit(retcode);
}
