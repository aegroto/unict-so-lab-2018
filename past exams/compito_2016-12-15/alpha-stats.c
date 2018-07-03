#include <stdlib.h>
#include <stdio.h>       
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define MEM_SIZE sizeof(int) * 26

#define SEM_FATHER 0
#define SEM_CHILD 1

int WAIT(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_des, operazioni, 1);
}

/************************************/

/************/
/** FATHER **/
/************/

int father(int* memoryPointer, int semFd, int* frequencies) {
    printf("I'm a father instance\n"); 

    WAIT(semFd, SEM_FATHER);

    memset(memoryPointer, 0, MEM_SIZE);

    SIGNAL(semFd, SEM_CHILD);

    WAIT(semFd, SEM_FATHER);

    // printf("[father] received: ");
    for(int i = 0; i < 26; ++i) {
        frequencies[i] += memoryPointer[i];
    }
    // printf("\n");

    SIGNAL(semFd, SEM_FATHER);

    return 0;
}

/***********/
/** CHILD **/
/***********/

int child(int* memoryPointer, char* filePath, int semFd) {
    printf("I'm a child (%s)\n", filePath);

    WAIT(semFd, SEM_CHILD);
    int fd;
    if((fd = open(filePath, O_RDONLY)) == -1) {
        perror("[child] fd");
        return 1;
    }

    struct stat fileStats;
    stat(filePath, &fileStats);

    char* filePointer;
    if((filePointer = mmap(NULL, fileStats.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == NULL) {
        perror("[child] mmap");
        return 1;
    }

    size_t readBytes = 0;
    while(readBytes < fileStats.st_size) {
        if(filePointer[readBytes] >= 'A' && filePointer[readBytes] <= 'z') {
            int index = toupper(filePointer[readBytes]) - 'A';
            ++memoryPointer[index];
        }

        ++readBytes;
    }

    /*printf("[child] sending: ");
    for(int i = 0; i < 26; ++i) {
        printf("%d ", memoryPointer[i]);
    }
    printf("\n");*/

    SIGNAL(semFd, SEM_FATHER);
    return 0;
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("usage: %s <file-1> [file-2] ...\n", argv[0]);
        return 1;
    }

    int memoryFd;
    if((memoryFd = shmget(IPC_PRIVATE, MEM_SIZE, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
        perror("shmget");
        return 1;
    }

    int* memoryPointer;
    if((memoryPointer = shmat(memoryFd, NULL, 0)) == (int*) -1) {
        perror("shmat");
        return 1;
    }

    int semFd;
    if((semFd = semget(IPC_PRIVATE, 2, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
        perror("semget");
        return 1;
    }

    short semvals[2] = { 1, 0 };
    if(semctl(semFd, 0, SETALL, semvals) == -1) {
        perror("semctl");
        return 1;
    }

    int pid = 0;
    int numChild = 0;
    int retcode = 0;

    int frequencies[26] = { 0 };
    
    while(numChild < argc - 1) {
        pid = fork();
        if(pid != 0) { // father
            retcode = father(memoryPointer, semFd, frequencies);
        } else { // child
            retcode = child(memoryPointer, argv[numChild+1], semFd);
            return retcode;
        }
        ++numChild;    
    }

    printf("frequenze: \n");
    for(int i = 0; i < 26; ++i) {
        printf("%c = %d \n", i +'a', frequencies[i]);
    }

    if(shmctl(memoryFd, IPC_RMID, 0) != 0) {
        perror("shmctl");
        return 1;
    }

    return 0;
}
