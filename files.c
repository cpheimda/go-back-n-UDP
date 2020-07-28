#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

#include "files.h"

char* readSingleFile(char* filename){
    FILE *f;
    f = fopen(filename, "r");
    if(!f){
        fprintf(stderr, "Error opening single file |%s|\n.", filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long filsize = ftell(f);
    //rintf("file %s contains: %ld bytes\n", filename, filsize);
    fseek(f, 0, SEEK_SET);
    char *bilde = malloc(filsize + 1);
    fread(bilde, 1, filsize, f);
    fclose(f);
    bilde[filsize] = 0;
    //printf("Skriver ut fil: \n %s", bilde);
    return bilde;
}