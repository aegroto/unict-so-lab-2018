/*
    Homework n.1

    Scrivere un programma in linguaggio C che permetta di copiare un numero
    arbitrario di file regolari su una directory di destinazione preesistente.

    Il programma dovra' accettare una sintassi del tipo:
     $ homework-1 file1.txt path/file2.txt "nome con spazi.pdf" directory-destinazione
*/

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    char *directory = argv[argc - 1];
    struct stat stats = { 0 };

    if (stat(directory, &stats) == -1) {
        mkdir(directory, 0700);
    }

    const int lastFileIndex = argc - 2;
    char filePath[100];
    int rfd, wfd;
    for(int i = 1; i <= lastFileIndex; ++i) {
        sprintf(filePath, "%s%s", directory, argv[i]);

        if(lstat(argv[i], &stats) != -1 && 
            !S_ISDIR(stats.st_mode) && S_ISREG(stats.st_mode)) {
            rfd = open(argv[i], O_RDONLY);
            wfd = creat(filePath, S_IWUSR | S_IRUSR | S_IXUSR);

            char fileContent[stats.st_size];
            if(read(rfd, fileContent, stats.st_size) > 0) {
                write(wfd, fileContent, stats.st_size);
            } else {
                fprintf(stderr, "Unable to read content of file: %s\n", argv[i]);
            }
        } else {
            fprintf(stderr, "Unable to copy file: %s\n", argv[i]);
        }
    } 
}
