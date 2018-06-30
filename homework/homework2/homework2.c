/*
    Homework n.2

    Estendere l'esempio 'move.c' visto a lezione per supportare i 2 casi speciali:
    - spostamento cross-filesystem: individuato tale caso, il file deve essere
     spostato utilizzando la strategia "copia & cancella";
    - spostamento di un link simbolico: individuato tale caso, il link simbolico
     deve essere ricreato a destinazione con lo stesso contenuto (ovvero il percorso
     che denota l'oggetto referenziato); notate come tale spostamento potrebbe
     rendere il nuovo link simbolico non effettivamente valido.

    La sintassi da supportare e' la seguente:
     $ homework-2 <pathname sorgente> <pathname destinazione>
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>

int main(int argc, char** argv) {
    if(argc != 3) {
        fprintf(stderr, "use: %s <pathname sorgente> <pathname destinazione>\n", argv[0]);
        exit(1);
    }

    /*size_t maxDestLength = strlen(argv[2]);
    char destFolder[maxDestLength];
    strcpy(destFolder, argv[2]);
    size_t i = maxDestLength - 1;
    while(destFolder[i] != '/' && i >= 0) {
        destFolder[i--] = 0;
    }*/
    
    char destFolder[strlen(argv[2])];
    strcpy(destFolder, argv[2]);
    basename(destFolder);

    struct stat sourceStats, destStats;
    if(lstat(argv[1], &sourceStats) == -1) {
        fprintf(stderr, "Error analyzing source pathname: %s\n", argv[1]);
        exit(1);
    }

    if(lstat(destFolder, &destStats) == -1) {
        fprintf(stderr, "Error analyzing destination pathname: %s\n", destFolder);
        exit(1);
    }

    if(S_ISDIR(sourceStats.st_mode)) {
        fprintf(stderr, "Source pathname is a directory\n");
        exit(1);
    }

    if(S_ISREG(sourceStats.st_mode)) {
        if (link(argv[1], argv[2]) == -1) {
            perror(argv[2]);
            printf("Error linking file\n");
            exit(1);
        }

        if (unlink(argv[1]) == -1) {
            perror(argv[1]);
            printf("Error unlinking file\n");
            exit(1);
        }
    } else if(S_ISLNK(sourceStats.st_mode) || sourceStats.st_dev != destStats.st_dev) {
        int rfd, wfd;
        rfd = open(argv[1], O_RDONLY);
        wfd = creat(argv[2], S_IWUSR | S_IRUSR | S_IXUSR);

        char fileContent[sourceStats.st_size];
        if(read(rfd, fileContent, sourceStats.st_size) > 0) {
            write(wfd, fileContent, sourceStats.st_size);
        } else {
            fprintf(stderr, "Unable to read content of file: %s\n", argv[1]);
        }

        if(remove(argv[1]) == -1) {
            fprintf(stderr, "Unable to remove source file: %s (destination file has been already created)\n", argv[1]);
        }
    } else {
        fprintf(stderr, "Unsupported file type\n");
    }
}