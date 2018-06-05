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
                rfd = open(argv[i], O_RDONLY);
                wfd = creat(filePath, S_IWUSR | S_IRUSR | S_IXUSR);

                char fileContent[stats.st_size];
                if(read(rfd, fileContent, stats.st_size) > 0) {
                    write(wfd, fileContent, stats.st_size);
                } else {
                    fprintf(stderr, "Unable to read content of file: %s\n", argv[i]);
                }
            } else if(S_ISDIR(stats.st_mode)) {

            }
        } else {
            fprintf(stderr, "Unable to copy file: %s\n", argv[i]);
        }
    } 
}
