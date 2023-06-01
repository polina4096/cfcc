#ifndef CFCC_UTIL_C
#define CFCC_UTIL_C

#include <stdio.h>
#include <stdlib.h>

char* read_file(char* filename) {
    FILE* f = fopen(filename, "rt");
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = (char*) malloc(length + 1);
    buffer[length] = '\0';

    fread(buffer, 1, length, f);
    
    fclose(f);
    return buffer;
}

#endif