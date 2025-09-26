#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "file.h"

/**
 * Loads a file into memory and returns a pointer to the data.
 * 
 * Buffer is not NULL-terminated.
 */
struct file_data *file_load(char *filename)
{
    char *buffer, *p;
    struct stat buf;
    int bytes_read, bytes_remaining, total_bytes = 0;
    
    // Get the file size
    if (stat(filename, &buf) == -1) {
        return NULL;
    }

    // Make sure it's a regular file
    if (!(buf.st_mode & S_IFREG)) {
        return NULL;
    }

    // Open the file for reading
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        return NULL;
    }

    // Allocate that many bytes
    bytes_remaining = buf.st_size;
    printf("bytes remaining before %d\n", bytes_remaining);
    p = buffer = malloc(buf.st_size);

    if (buffer == NULL) {
        return NULL;
    }

    //this is where buffer overflow happens. For sure...
    // Read in the entire file
    while ((bytes_read = fread(p, 1, bytes_remaining, fp))> 0) {
        printf("bytes read %d\n", bytes_read);
        bytes_remaining -= bytes_read;
        p += bytes_read;
        total_bytes += bytes_read;
    }
    fclose(fp);

    // Allocate the file data struct
    struct file_data* filedata = malloc(sizeof(struct file_data));
    
    //init values
    filedata->data = NULL;
    filedata->size = 0;

    printf("%p",filedata);
    if (filedata == NULL) {
        free(buffer);
        return NULL;
    }

    filedata->data = buffer;
    filedata->size = total_bytes;
    
    return filedata;
}

/**
 * Frees memory allocated by file_load().
 */
void file_free(struct file_data *filedata)
{
    free(filedata->data);
    free(filedata);
}