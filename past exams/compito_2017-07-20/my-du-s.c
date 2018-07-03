#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <dirent.h>
#include <string.h>

#define MEM_SIZE 1024

#define MAX_PATH_LEN 512

#define STOP_SEQ "\1"

#define SEM_MUTEX 0
#define SEM_SCANNER 1
#define SEM_FATHER 2
#define SEM_STATER 3

int WAIT(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_des, operazioni, 1);
}

void crawlDir(char* dirPath, char* memoryPointer, int semFd) {
    DIR *dir;

    if((dir = opendir(dirPath)) == NULL) {
        perror("opendir");
    }

    // printf("[scanner] scanning directory %s...\n", dirPath);

    struct dirent *dirEntry = readdir(dir);

    while(dirEntry != NULL) {
        if(strcmp(dirEntry->d_name, ".") != 0 && strcmp(dirEntry->d_name, "..") != 0) {
            char fullPath[MEM_SIZE];
            if(dirPath[strlen(dirPath)-1] == '/')
                sprintf(fullPath, "%s%s", dirPath, dirEntry->d_name);
            else
                sprintf(fullPath, "%s/%s", dirPath, dirEntry->d_name);

            if(dirEntry->d_type == DT_DIR) {
                printf("[scanner] found new directory %s\n", fullPath);
                crawlDir(fullPath, memoryPointer, semFd);
            } else if(dirEntry->d_type == DT_REG) {
                printf("[scanner] found new file %s\n", fullPath);

                WAIT(semFd, SEM_SCANNER);
                WAIT(semFd, SEM_MUTEX);

                strcpy(memoryPointer, fullPath);

                SIGNAL(semFd, SEM_MUTEX);
                SIGNAL(semFd, SEM_STATER);
            }
        }

        dirEntry = readdir(dir);
    }
}

int stater(char* memoryPointer, int numScanners, int semFd) {
    printf("I'm the stater\n");
    struct stat fileStats;
    char buffer[MEM_SIZE];

    int currentScanners = numScanners;
    while(numScanners > 0) {
        WAIT(semFd, SEM_STATER);
        WAIT(semFd, SEM_MUTEX);

        printf("[stater] received %s\n", memoryPointer);
        if(strcmp(memoryPointer, STOP_SEQ) != 0) {
            stat(memoryPointer, &fileStats); 

            sprintf(buffer, "%lu", fileStats.st_blocks);  
            printf("[stater] buffer is %s\n", buffer); 
            strcpy(memoryPointer + MAX_PATH_LEN, buffer);
        } else {
            --numScanners;
            if(numScanners == 0) {
                strcpy(memoryPointer, STOP_SEQ);
            }
        }

        SIGNAL(semFd, SEM_MUTEX);
        SIGNAL(semFd, SEM_FATHER);
    }
    return 0;
}

int father(char* memoryPointer, int semFd) {
    printf("I'm the father\n");

    int run = 1;
    while(run) {
        WAIT(semFd, SEM_FATHER);
        WAIT(semFd, SEM_MUTEX);

        printf("[father] received %s, %s\n", memoryPointer, memoryPointer + MAX_PATH_LEN);
        run = strcmp(memoryPointer, STOP_SEQ);

        SIGNAL(semFd, SEM_MUTEX);
        SIGNAL(semFd, SEM_SCANNER);
    }
    return 0;
}

int scanner(char* dirPath, char* memoryPointer, int semFd) {
    printf("I'm a scanner (%s)\n", dirPath);

    crawlDir(dirPath, memoryPointer, semFd);

    WAIT(semFd, SEM_SCANNER);
    WAIT(semFd, SEM_MUTEX);

    strcpy(memoryPointer, STOP_SEQ);

    SIGNAL(semFd, SEM_MUTEX);
    SIGNAL(semFd, SEM_STATER); 

    return 0;
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("usage: %s [path-1] [path-2] ...\n", argv[0]);
        return 1;
    }

    int memoryFd;
    if((memoryFd = shmget(IPC_PRIVATE, MEM_SIZE, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
        perror("shmget");
        return 1;
    }

    char* memoryPointer;
    if((memoryPointer = shmat(memoryFd, NULL, 0)) == (void*) -1) {
        perror("shmat");
        return 1;
    }

    int semFd;
    if((semFd = semget(IPC_PRIVATE, 4, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
        perror("semget");
        return 1;
    }

    short semvals[4] = { 1, 1, 0, 0 };

    if(semctl(semFd, 0, SETALL, semvals) != 0) {
        perror("semctl");
        return 1;
    }

    int pid;

    pid = fork();
    
    if(pid != 0) { // father
        for(int i = 0; i < argc - 1 && pid != 0; ++i) {
            pid = 0;
            pid = fork();

            if(pid == 0) {
                scanner(argv[i+1], memoryPointer, semFd);
                break;
            }
        }

        if(pid != 0) {            
            father(memoryPointer, semFd);
        }

        if(shmctl(memoryFd, IPC_RMID, NULL) != 0) {
            perror("shmctl");
            return 1;
        }
    } else { // stater
        stater(memoryPointer, argc - 1, semFd);
    }
    return 0;
}
