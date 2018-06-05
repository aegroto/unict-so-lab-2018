/*****************************************************/
/**        OS Lab homeworks 2018 repository         **/
/**           UNICT - Lorenzo Catania               **/
/**      Parts of this code may be taken from       **/
/**       Prof. Mario Di Raimondo's solutions       **/
/*****************************************************/

/*
    Homework n.4

    Estendere l'esercizio 'homework n.1' affinche' operi correttamente
    anche nel caso in cui tra le sorgenti e' indicata una directory, copiandone
    il contenuto ricorsivamente. Eventuali link simbolici incontrati dovranno
    essere replicati come tali (dovrà essere creato un link e si dovranno
    preservare tutti permessi di accesso originali dei file e directory).

    Una ipotetica invocazione potrebbe essere la seguente:
     $ homework-4 directory-di-esempio file-semplice.txt path/altra-dir/ "nome con spazi.pdf" directory-destinazione
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

int copy_reg_file(char* path, char* destPath, size_t fsize) {
    printf("Copying file %s...\n", path);
    int rfd = open(path, O_RDONLY);
    int wfd = creat(destPath, S_IWUSR | S_IRUSR | S_IXUSR);

    char fileContent[fsize];
    if(read(rfd, fileContent, fsize) > 0) {
        write(wfd, fileContent, fsize);
    } else {
        return -1;
    }

    return 0;
}

int copy_directory(char* path, char* destPath) {
    DIR* dirstream = opendir(path);
    struct dirent* direntry = NULL;
    struct stat fileStats = { 0 };
    char filePath[100];

    while((direntry = readdir(dirstream)) != NULL) {
        sprintf(filePath, "%s%s", path, direntry->d_name);
        if (stat(filePath, &fileStats) == -1) {
            fprintf(stderr, "Error reading directory entry %s\n", filePath);
        }

        copy_reg_file(filePath, destPath, fileStats.st_size);
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "utilizzo: %s <file1.txt> || <path/file2.txt> || <\"nome con spazi.pdf\"> <directory-destinazione>\n", argv[0]);
        exit(1);
    }

    char *directory = argv[argc - 1];
    struct stat stats = { 0 };

    if (stat(directory, &stats) == -1) {
        if(mkdir(directory, 0700) == -1) {
            fprintf(stderr, "Not enough permissions on destination folder\n");
            exit(1);
        }
    }

    const int lastFileIndex = argc - 2;
    char filePath[100];
    int rfd, wfd;

    for(int i = 1; i <= lastFileIndex; ++i) {
        sprintf(filePath, "%s%s", directory, argv[i]);

        if(lstat(argv[i], &stats) != -1) { 
            if(S_ISREG(stats.st_mode)) {
                if(copy_reg_file(argv[i], filePath, stats.st_size) != 0) {
                    fprintf(stderr, "Unable to read content of file: %s\n", argv[i]);
                }
            } else if(S_ISDIR(stats.st_mode)) {
                copy_directory(argv[i], filePath);
            }
        } else {
            fprintf(stderr, "Unable to copy file: %s\n", argv[i]);
        }
    } 
}
