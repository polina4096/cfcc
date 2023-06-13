#ifndef CFCC_UTIL_C
#define CFCC_UTIL_C

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char* read_file(char* filename) {
    FILE* f = fopen(filename, "rt");
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(length + 1);
    buffer[length] = '\0';

    fread(buffer, 1, length, f);
    
    fclose(f);
    return buffer;
}

static int strapp(char** buffer_ptr, char* appendage) {
    size_t length_appendage = strlen(appendage);

    char* ptr;
    if (*buffer_ptr != NULL) {
        ptr = realloc(*buffer_ptr, strlen(*buffer_ptr) + length_appendage + 1);
        strcat(ptr, appendage);
    } else {
        ptr = malloc(length_appendage + 1);
        memcpy(ptr, appendage, length_appendage);
        ptr[length_appendage] = '\0';
    }

    if (ptr != NULL) {
        *buffer_ptr = ptr;
        return 0; // Success
    } else {
        return -1; // Failed to reallocate
    }
}

static int strfmt(char** buffer_ptr, const char* format, ...) {
    va_list args;
    va_start(args, format);    
    int length = vsnprintf(NULL, 0, format, args);
    char* temp = malloc(length + 1);
    va_end(args);

    va_start(args, format);
    vsnprintf(temp, length + 1, format, args);
    va_end(args);
    
    int result = strapp(buffer_ptr, temp);
    free(temp);

    return result;
}

#endif