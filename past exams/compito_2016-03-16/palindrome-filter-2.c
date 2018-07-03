#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define END_CHAR 0
#define SEP_CHAR 10

/****************/
/** PALINDROME **/
/****************/

int isPalindrome(char* string) {
    size_t len = strlen(string);
    for(size_t i = 0; i < len/2; ++i) {
        if(string[i] != string[len - 1- i])
            return 0;
    }

    return 1;
}

/************/
/** READER **/
/************/
int reader(char* filePath, int outPipeFd) {
    printf("I'm the reader (%s)\n", filePath);

    int fd;
    if((fd = open(filePath, O_RDONLY)) == -1) {
        perror("fd");
        return 1;
    }

    struct stat fileStats;
    stat(filePath, &fileStats);

    if(!S_ISREG(fileStats.st_mode)) {
        perror("input is not a regular file");
        return 1;
    }

    char* memoryPointer;
    if((memoryPointer = mmap(NULL, fileStats.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    size_t writtenBytes = 0;
    while(writtenBytes < fileStats.st_size) {
        writtenBytes += write(outPipeFd, memoryPointer, fileStats.st_size);
        printf("[reader] wrote %lu bytes\n", writtenBytes);
    }

    return 0;
}

/************/
/** FATHER **/
/************/

int father(char* filePath, int inPipeFd, int outPipeFd) {
    printf("I'm the father\n");

    struct stat fileStats;
    stat(filePath, &fileStats);

    char buffer[fileStats.st_size];

    size_t readBytes = 0;
    while(readBytes < fileStats.st_size) {
        readBytes += read(inPipeFd, buffer, PIPE_BUF);
        // printf("[father] read %lu bytes\n", readBytes);
    }

    // printf("[father] file content is: \"%s\"\n", buffer);

    char* token = strtok(buffer, "\n");
    
    while(token != NULL) {
        // printf("[father] parsing: %s\n", token);
        if(isPalindrome(token)) {
            size_t tokenLen = strlen(token);
            token[tokenLen] = SEP_CHAR;
            write(outPipeFd, token, tokenLen + 1);
            // printf("[father] this is palindrome: %s\n", token);
        }
        token = strtok(NULL, "\n");
    }

    write(outPipeFd, END_CHAR, sizeof(char));

    return 0;
}

/************/
/** WRITER **/
/************/

int writer(int inPipeFd) {
    printf("I'm the writer\n");

    char buffer[PIPE_BUF];

    int run = 1;
    size_t readBytes = 0;
    
    while(run) {
        readBytes = read(inPipeFd, buffer, PIPE_BUF);
        
        // printf("[writer] read: %s\n", buffer);

        size_t currentReadBytes = 0;
        while(currentReadBytes < readBytes) {
            if(buffer[currentReadBytes] != END_CHAR) {
                fprintf(stdout, "%c", buffer[currentReadBytes]);
            } else {
                run = 0;
            }

            ++currentReadBytes;
        }        
    }

    return 0;
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    int inPipeFd[2], outPipeFd[2];

    if(pipe(inPipeFd) == -1) {
        perror("pipe");
    }

    if(pipe(outPipeFd) == -1) {
        perror("pipe");
    }
    
    if(fork() == 0) { // writer
        close(inPipeFd[0]);
        close(inPipeFd[1]);
        // close(outPipeFd[1]);

        writer(outPipeFd[0]);
    } else if(fork() == 0) { // reader
        close(inPipeFd[0]);
        close(outPipeFd[0]);
        close(outPipeFd[1]);

        reader(argv[1], inPipeFd[1]);
    } else { // father
        // close(inPipeFd[1]);
        close(outPipeFd[0]);

        father(argv[1], inPipeFd[0], outPipeFd[1]);
    }

    return 0;
}
