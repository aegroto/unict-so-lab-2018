#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>

#define END_CHAR 255

/**********************/
/** PALINDROME CHECK **/
/**********************/

int isPalindrome(char* string) {
    size_t len = strlen(string);
    for(int i = 0; i < len/2; ++i) {
        if(tolower(string[i]) != tolower(string[len - 1 - i])) {
            return 0;
        }
    }
    return 1;
}

/************/
/** READER **/
/************/

int reader(char* filePath, char* outPipePath) {
    printf("I'm the reader\n");

    int inputFileFd, outPipeFd;

    if((outPipeFd = open(outPipePath, O_WRONLY)) == -1) {
        perror("[reader] output pipe");
        return 1;
    }

    if((inputFileFd = open(filePath, O_RDONLY)) == -1) {
        perror("[reader] input file");
        return 1;
    }

    struct stat fileStats;
    if(stat(filePath, &fileStats) != 0) {
        perror("[reader] stat");
        return 1;
    }

    char* mapPointer;
    if((mapPointer = mmap(mapPointer, fileStats.st_size, PROT_READ, MAP_PRIVATE, inputFileFd, 0)) == MAP_FAILED) {
        perror("[reader] mmap");
        return 1;
    }

    int writtenBytes = 0;
    char* token = strtok(mapPointer, "\n");
    printf("[reader] first token is %s\n", token);

    /*while(token != NULL) {
        write(outPipeFd, token, strlen(token) + 1);
        printf("[reader] written %s\n", token);
        token = strtok(NULL, "\n");
    }*/

    printf("[reader] finish\n");
    write(outPipeFd, "\END_CHAR", sizeof(char));

    return 0;
}

/************/
/** FATHER **/
/************/

int father(char* inPipePath, char* outPipePath) {
    printf("I'm the father\n");

    /*int inPipeFd, outPipeFd;

    if((inPipeFd = open(inPipePath, O_RDWR)) == -1) {
        perror("[father] input pipe");
        return 1;
    }

    if((outPipeFd = open(outPipePath, O_RDWR)) == -1) {
        perror("[father] output pipe");
        return 1;
    }

    char buffer[255];
    int run = 1;
    do {
        printf("[father] listening to pipe...\n");
        read(inPipeFd, buffer, sizeof(char) * 255);
        printf("[father] received \"%s\"\n", buffer);

        while(buffer[0] != END_CHAR) {
            if(isPalindrome(buffer)) {
                write(outPipeFd, buffer, strlen(buffer) + 1);
                printf("[father] wrote %s\n", buffer);
            }
        }
    } while(run);

    printf("[father] finish\n");
    write(outPipeFd, "\END_CHAR", sizeof(char));*/

    return 0;
}

/************/
/** WRITER **/
/************/
int writer(char* inPipePath) {
    printf("I'm the writer\n");
    /*int inPipeFd;

    if((inPipeFd = open(inPipePath, O_RDWR)) == -1) {
        perror("[writer] input pipe");
        return 1;
    }

    char buffer[255];
    do {
        read(inPipeFd, buffer, sizeof(char) * 255);
        fprintf(stdout, "[outputer] %s\n", buffer);
    } while(buffer[0] != END_CHAR);

    printf("[writer] finish\n");*/
    return 0;
}

/**********/
/** MAIN **/
/**********/

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("usage: %s <input-file>\n", argv[0]);
        exit(1);
    }
    
    char *inPipePath = (char*) malloc(sizeof(char) * 255),
         *outPipePath = (char*) malloc(sizeof(char) * 255);

    sprintf(inPipePath, "inpipe"/*, argv[0]*/);
    sprintf(outPipePath, "outpipe"/*, argv[0]*/);

    if((mkfifo(inPipePath, 0600)) != 0) {
        perror("input pipe");
        exit(1);
    }

    if((mkfifo(outPipePath, 0600)) != 0){
        perror("output pipe");
        exit(1);
    }
    int retcode = 0;

    int writerPid;

    writerPid = fork();

    if(writerPid != 0) { // father
        int readerPid = fork();

        if(readerPid!= 0) { // father
            retcode = father(inPipePath, outPipePath);
            /*if(kill(writerPid, 0) == 0) {
                wait(NULL);
            } else if(kill(readerPid, 0) == 0) {
                wait(NULL);
            }*/
            wait(NULL);

            if(unlink(inPipePath) != 0) {
                perror("error unlinking input pipe");
                exit(1);
            }

            if(unlink(outPipePath) != 0) {
                perror("error unlinking input pipe");
                exit(1);
            }
        } else { // reader
            retcode = reader(argv[1], inPipePath);
        }
    } else { // writer
        retcode = writer(outPipePath);
    }

    exit(retcode);
}
