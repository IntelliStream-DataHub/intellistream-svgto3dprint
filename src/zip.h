/* Minimal ZIP writer (stored entries) used for the 3MF container. */
#ifndef LOGO3D_ZIP_H
#define LOGO3D_ZIP_H

#include <stdio.h>
#include <stddef.h>

typedef struct zip_entry {
    char *name;
    unsigned crc;
    unsigned size;
    unsigned offset;
} zip_entry;

typedef struct {
    FILE *f;
    zip_entry *entries;
    int nentries;
} zip_writer;

int zip_open(zip_writer *z, const char *path);
int zip_add(zip_writer *z, const char *name, const void *data, size_t len);
int zip_close(zip_writer *z);
unsigned zip_crc32(const void *data, size_t len);

#endif
