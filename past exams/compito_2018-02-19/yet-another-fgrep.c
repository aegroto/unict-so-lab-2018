#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>

#define BUFFER_SIZE PIPE_BUF

int reader(char* fileName, int pipeFd) {
    // printf("I'm a reader (%s)\n", fileName);
    int fd = open(fileName, O_RDONLY);
    
    struct stat fileStats;
    
    stat(fileName, &fileStats);
    
    void* mapPointer;
    if((mapPointer = mmap(mapPointer, fileStats.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == 0) {
        perror("mmap");
        return 1;
    }
    
    int fileIndex = 0, writtenBytes = 0;
    char buffer[BUFFER_SIZE];
    
    while(writtenBytes < fileStats.st_size) {        
        // printf("[reader] writing from index: %d (%d)\n", fileIndex, pipeFd);
        writtenBytes += write(pipeFd, (char*) mapPointer, BUFFER_SIZE);
        fileIndex += writtenBytes;
        mapPointer += writtenBytes;
        // printf("[reader] currentIndex: %d\n", fileIndex);
    }
    
    buffer[0] = 0;
    write(pipeFd, buffer, sizeof(char));
    
    return 0;
}

int father(char* word, int inPipeFd, int outPipeFd, int reverse, int caseSensitive) {
    // printf("I'm a father\n");    
    char buffer[BUFFER_SIZE], tmp[BUFFER_SIZE];
    int finish = 0;
    
    while(!finish) {
        // printf("[father] listening on pipe (%d)...\n", inPipeFd);
        read(inPipeFd, buffer, BUFFER_SIZE);
        // printf("[father] received \"%s\"\n", buffer);
        
        int lastIndex = 0, currentIndex = 0;
        while(buffer[lastIndex] != 0 && lastIndex < BUFFER_SIZE) {
            tmp[currentIndex] = buffer[lastIndex];
            
            if(tmp[currentIndex] == 10) {
                tmp[currentIndex] = 0;
                int condition = 
                    caseSensitive && (strstr(tmp, word) || (reverse && !strstr(tmp, word))) ||
                    (strcasestr(tmp, word) || (reverse && !strcasestr(tmp, word)));
                    
                if(condition) {
                    printf("[father] writing \"%s\"\n", tmp);
                    write(outPipeFd, tmp, strlen(tmp));
                    write(outPipeFd, "\n", sizeof(char));
                    ++lastIndex;
                }
                currentIndex = 0;
            } else ++currentIndex;
            
            lastIndex++;
        }
        
        finish = buffer[0] == 0;
    }
    
    return 0;
}

int outputer(int inPipeFd) {
    // printf("I'm an outputer\n");
    char buffer[BUFFER_SIZE];
    
    int finish = 0;
    while(!finish) {
        read(inPipeFd, buffer, BUFFER_SIZE);
        fprintf(stdout, "[outputer] %s", buffer); 
    }
    return 0;
}

int main(int argc, char** argv) {
    if(argc < 3) {
        printf("usage:  %s [-v] [-i] [word] <file-1> [file-2] ...\n", argv[0]);
        return 1;
    }
    
    int reverse = (strcmp(argv[1], "-v") == 0) || (strcmp(argv[2], "-v") == 0);
    int caseSensitive = !(strcmp(argv[1], "-i") == 0) || (strcmp(argv[2], "-i") == 0);
    
    char** files = argv + 1;
    int f;
    for(f = 0; f < argc && files[0][0] == '-'; ++f) {
        ++files;
    }
    
    int filesCount = argc - 2 - f; 
    
    if(filesCount <= 0) {
        printf("not enough arguments, need a word and at least a file\n");
        return 1;
    }
    
    files = files + 1;
    
    int* inPipeFd = (int*) malloc(sizeof(int) * 2);
    int* outPipeFd = (int*) malloc(sizeof(int) * 2);
    
    if(pipe(inPipeFd) != 0) {
        perror("input pipe");
        exit(1);
    }
    
    if(pipe(outPipeFd) != 0) {
        perror("output pipe");
        exit(1);
    }
    
    int pid = fork();
    
    if(pid != 0) { // father        
        // printf("file count: %d\n", filesCount);
        
        int i = 0;
        while(i < filesCount) {
            pid = fork();            
            if(pid != 0) {
                father(files[-1], inPipeFd[0], outPipeFd[1], reverse, caseSensitive);
                wait(NULL);
                pid = 0;
            } else {
                return reader(files[i], inPipeFd[1]);
            }            
            ++i;
        }
        
        /*do {            
            ++i;
            pid = fork();
            if(pid == 0) {
                reader();
            }
        } while(pid != 0 && i < filesCount);*/
    } else { // outputer
        outputer(outPipeFd[0]);
    } 
    
    return 0;
}