#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define SEM_MUTEX 0
#define SEM_EMPTY 1
#define SEM_FULL 2

#define CONT_CHAR 0
#define STOP_CHAR 1

int WAIT(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_des, operazioni, 1);
}

int child(int fileFd, int memoryFd, int semFd) {
    printf("I'm the child\n");

    char* memoryPointer;
    if((memoryPointer = shmat(memoryFd, NULL, 0)) == ((void*) -1)) {
        perror("shmat");
        return 1;
    }

    char buffer[2];
    while(read(fileFd, &buffer, sizeof(char)) > 0) {
        // printf("[child] read %c\n", buffer[0]);
        WAIT(semFd, SEM_EMPTY);
        WAIT(semFd, SEM_MUTEX);
        
        // buffer[1] = CONT_CHAR;
        // write(memoryFd, buffer, sizeof(buffer));
        // printf("[child] wrote %c%c (%d, %d)\n", buffer[0], buffer[1], buffer[0], buffer[1]);

        // printf("[child] writing %c...\n", buffer[0]);
        memoryPointer[0] = buffer[0];
        memoryPointer[1] = CONT_CHAR;
        // printf("[child] wrote %c...\n", buffer[0]);

        SIGNAL(semFd, SEM_MUTEX);
        SIGNAL(semFd, SEM_FULL);
    }

    WAIT(semFd, SEM_EMPTY);
    WAIT(semFd, SEM_MUTEX);

    memoryPointer[0] = 0;
    memoryPointer[1] = STOP_CHAR;

    SIGNAL(semFd, SEM_MUTEX);
    SIGNAL(semFd, SEM_FULL);

    return 0;
}

int father(int memoryFd, int semFd) {
    printf("I'm the father\n");

    char* memoryPointer;
    if((memoryPointer = shmat(memoryFd, NULL, 0)) == (void*) -1) {
        perror("shmat");
        return 1;
    }

    size_t charNumber = 0, wordNumber = 0, lineNumber = 0;

    while(memoryPointer[1] != STOP_CHAR) {
        WAIT(semFd, SEM_FULL);
        WAIT(semFd, SEM_MUTEX);

        // printf("[father] read %c%c\n", memoryPointer[0], memoryPointer[1]);
        ++charNumber;
        if(memoryPointer[0] == ' ' || memoryPointer[0] == ',' || memoryPointer[0] == '.')
            ++wordNumber;
        else if(memoryPointer[0] == 10)
            ++lineNumber;

        SIGNAL(semFd, SEM_MUTEX);
        SIGNAL(semFd, SEM_EMPTY);
    }

    --charNumber;
    printf("chars: %lu, words: %lu, lines: %lu\n", charNumber, wordNumber, lineNumber);

    return 0;
}

int main(int argc, char** argv) {
    int memoryFd;
    if((memoryFd = shmget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
        perror("shmget");
        return 1;
    }

    int semFd;
    if((semFd = semget(IPC_PRIVATE, 3, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
        perror("semget");
        return 1;
    }

    if(semctl(semFd, SEM_MUTEX, SETVAL, 1) != 0) {
        perror("semctl");
        return 1;
    }

    if(semctl(semFd, SEM_EMPTY, SETVAL, 1) != 0) {
        perror("semctl");
        return 1;
    }

    if(semctl(semFd, SEM_FULL, SETVAL, 0) != 0) {
        perror("semctl");
        return 1;
    }

    int pid = fork();

    if(pid != 0) {
        father(memoryFd, semFd);

        if(semctl(semFd, 0, IPC_RMID, 0) != 0) {
            perror("semctl");
            return 0;
        }

        if(shmctl(memoryFd, IPC_RMID,  NULL) != 0) {
            perror("shmctl");
            return 0;
        }
    } else {
        int fileFd;

        if(argc > 1) {
            if((fileFd = open(argv[1], O_RDONLY)) == -1) {
                perror("open");
                return 1;
            }
        } else {
            fileFd = 0;
        }
        child(fileFd, memoryFd, semFd);
    }


    return 0;
}
