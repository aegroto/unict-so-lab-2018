#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define MAX_WORD_LENGTH 255
#define SHM_FAILED ((int*) -1)

#define SEM_READERMEM_MUTEX 0
#define SEM_READERMEM_EMPTY 1
#define SEM_READERMEM_FULL 2
#define SEM_WRITERMEM_MUTEX 3
#define SEM_WRITERMEM_EMPTY 4
#define SEM_WRITERMEM_FULL 5

#define FINISH_CHAR 10

int WAIT(int sem_des, int num_semaforo){
  struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
  return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, int num_semaforo){
  struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
  return semop(sem_des, operazioni, 1);
}

int isPalindrome(char* word) {
  size_t firstCharIndex = 0, lastCharIndex = 0;
  while(word[lastCharIndex] != 0)
    ++lastCharIndex;

  --lastCharIndex;

  while(firstCharIndex < lastCharIndex) {
    if(word[firstCharIndex] != word[lastCharIndex])
      return 0;

    ++firstCharIndex;
    --lastCharIndex;
  }

  return 1;
}

int creator(char* readMemoryPointer, char* writeMemoryPointer, int semaphoresDescriptor) {
  printf("I'm the creator\n");
  int finish = 0;

  while(!finish) {
    WAIT(semaphoresDescriptor, SEM_READERMEM_FULL);
    WAIT(semaphoresDescriptor, SEM_READERMEM_MUTEX);

    if(readMemoryPointer[0] == FINISH_CHAR) {
      printf("[creator] received finish signal\n");
      finish = 1;
    } else {
      if(isPalindrome(readMemoryPointer)) {

        WAIT(semaphoresDescriptor, SEM_WRITERMEM_EMPTY);
        WAIT(semaphoresDescriptor, SEM_WRITERMEM_MUTEX);

        strcpy(writeMemoryPointer, readMemoryPointer);

        SIGNAL(semaphoresDescriptor, SEM_WRITERMEM_MUTEX);
        SIGNAL(semaphoresDescriptor, SEM_WRITERMEM_FULL);
      }
    }

    SIGNAL(semaphoresDescriptor, SEM_READERMEM_MUTEX);
    SIGNAL(semaphoresDescriptor, SEM_READERMEM_EMPTY);
  }

  WAIT(semaphoresDescriptor, SEM_WRITERMEM_EMPTY);
  WAIT(semaphoresDescriptor, SEM_WRITERMEM_MUTEX);

  writeMemoryPointer[0] = FINISH_CHAR;

  SIGNAL(semaphoresDescriptor, SEM_WRITERMEM_MUTEX);
  SIGNAL(semaphoresDescriptor, SEM_WRITERMEM_FULL);

  wait(NULL);
  wait(NULL);
  return 0;
}

int writer(int outputFileDescriptor, char* writeMemoryPointer, int semaphoresDescriptor) {
  printf("I'm the writer\n");

  FILE* outputFilePointer;
  if((outputFilePointer = fdopen(outputFileDescriptor, "w")) == NULL) {
    perror("[writer] error opening output file\n");
    return 1;  
  }

  int finish = 0;

  while(!finish) {
    WAIT(semaphoresDescriptor, SEM_WRITERMEM_FULL);
    WAIT(semaphoresDescriptor, SEM_WRITERMEM_MUTEX);

    if(writeMemoryPointer[0] == FINISH_CHAR) {
      printf("[writer] received finish signal\n");
      finish = 1;
    } else {
      size_t wordLength = strlen(writeMemoryPointer);
      writeMemoryPointer[wordLength] = 10;

      if(fwrite(writeMemoryPointer, sizeof(char), wordLength + 1, outputFilePointer) < wordLength) {
        perror("[writer] error writing in file");
        return 1;
      }      
    }

    SIGNAL(semaphoresDescriptor, SEM_WRITERMEM_MUTEX);
    SIGNAL(semaphoresDescriptor, SEM_WRITERMEM_EMPTY);
  }
  
  return 0;
}

int reader(int inputFileDescriptor, size_t inputSize, char* readMemoryPointer, int semaphoresDescriptor) {
  printf("I'm the reader\n");

  char *inputMapPointer;

  if((inputMapPointer = mmap(NULL, inputSize, PROT_READ, MAP_PRIVATE, inputFileDescriptor, 0)) == MAP_FAILED) {
    return 1;  
  }

  size_t seekIndex = 0;
  size_t wordSize;

  while(seekIndex < inputSize) {
    wordSize = 0;
    
    WAIT(semaphoresDescriptor, SEM_READERMEM_EMPTY);
    WAIT(semaphoresDescriptor, SEM_READERMEM_MUTEX);

    while(inputMapPointer[seekIndex + wordSize] != 10) {
      readMemoryPointer[wordSize] = inputMapPointer[seekIndex + wordSize];
      ++wordSize;
    }
    readMemoryPointer[wordSize + 1] = 0;

    // printf("[reader] wrote word %s (%lu/%lu)\n", readMemoryPointer, seekIndex + wordSize, inputSize);

    SIGNAL(semaphoresDescriptor, SEM_READERMEM_MUTEX);
    SIGNAL(semaphoresDescriptor, SEM_READERMEM_FULL);

    seekIndex += wordSize + 1;
  } 

  WAIT(semaphoresDescriptor, SEM_READERMEM_EMPTY);
  WAIT(semaphoresDescriptor, SEM_READERMEM_MUTEX);
  
  readMemoryPointer[0] = FINISH_CHAR;

  SIGNAL(semaphoresDescriptor, SEM_READERMEM_MUTEX);
  SIGNAL(semaphoresDescriptor, SEM_READERMEM_FULL);

  return 0;
}

int main(int argc, char** argv) {
  if(argc < 2 || argc > 3) {
    printf("usage: %s <input file> [output file]\n", argv[0]);
    return 1;
  } 

  int retcode = 0;

  // Creating shared memory fragments
  int readMemoryDescriptor;
  int writeMemoryDescriptor;

  if((readMemoryDescriptor = shmget(IPC_PRIVATE, MAX_WORD_LENGTH * sizeof(char), 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
    perror("error mapping shared read memory");
    return 1;
  }

  if((writeMemoryDescriptor = shmget(IPC_PRIVATE, MAX_WORD_LENGTH * sizeof(char), 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
    perror("error mapping shared write memory");
    return 1;
  }

  // Creating semaphores

  int semaphoresDescriptor;

  if((semaphoresDescriptor = semget(IPC_PRIVATE, 6, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
    perror("error creating semaphores");
    return 1;
  }

  semctl(semaphoresDescriptor, SEM_READERMEM_MUTEX, SETVAL, 1); 
  semctl(semaphoresDescriptor, SEM_READERMEM_EMPTY, SETVAL, 1); 
  semctl(semaphoresDescriptor, SEM_READERMEM_FULL, SETVAL, 0); 

  semctl(semaphoresDescriptor, SEM_WRITERMEM_MUTEX, SETVAL, 1); 
  semctl(semaphoresDescriptor, SEM_WRITERMEM_EMPTY, SETVAL, 1); 
  semctl(semaphoresDescriptor, SEM_WRITERMEM_FULL, SETVAL, 0); 

  // More operations

  void *readMemoryPointer, *writeMemoryPointer;

  if((readMemoryPointer = shmat(readMemoryDescriptor, NULL, 0)) == SHM_FAILED) {
    perror("error mapping read memory\n");
    return 1;    
  }

  if((writeMemoryPointer = shmat(writeMemoryDescriptor, NULL, 0)) == SHM_FAILED) {
    perror("error mapping write memory\n");
    return 1;    
  }

  int inputFileDescriptor, outputFileDescriptor;

  if((inputFileDescriptor = open(argv[1], O_RDONLY)) == -1) {
    perror("error opening input file\n");
    return -1;
  }

  if(argc == 2) {
    outputFileDescriptor = 1;
  } else {
    if((outputFileDescriptor = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0600)) == -1) {
      perror("error opening output file\n");
      return -1;
    }
  }

  struct stat inputFileStat, outputFileStat;
  if(stat(argv[1], &inputFileStat) == -1) {
    return 1;  
  }

  if(argc == 3) {
    if(stat(argv[2], &outputFileStat) == -1) {
      return 1;  
    }
  }

  int rpid, wpid;

  rpid = fork();

  if(rpid != 0) {
    wpid = fork();
    if(wpid != 0) {
      retcode = creator(readMemoryPointer, writeMemoryPointer, semaphoresDescriptor);
      if(retcode != 0) perror("error in creator\n");
    } else { 
      retcode = writer(outputFileDescriptor, writeMemoryPointer, semaphoresDescriptor);
      if(retcode != 0) perror("error in writer\n");
    }
  } else {
    retcode = reader(inputFileDescriptor, inputFileStat.st_size, readMemoryPointer, semaphoresDescriptor);
    if(retcode != 0) perror("error in reader\n");
  }

  // Removing shared spaces
  shmctl(readMemoryDescriptor, IPC_RMID, NULL);
  shmctl(writeMemoryDescriptor, IPC_RMID, NULL);
}
