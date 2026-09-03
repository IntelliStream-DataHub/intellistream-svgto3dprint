#include "zip.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned crc_table[256];
static int crc_ready = 0;

static void crc_init(void)
{
    unsigned i, j;
    for (i = 0; i < 256; i++) {
        unsigned c = i;
        for (j = 0; j < 8; j++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    crc_ready = 1;
}

unsigned zip_crc32(const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned c = 0xFFFFFFFFu;
    size_t i;
    if (!crc_ready) crc_init();
    for (i = 0; i < len; i++) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put16(FILE *f, unsigned v)
{
    unsigned char b[2] = {(unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF)};
    fwrite(b, 1, 2, f);
}

static void put32(FILE *f, unsigned v)
{
    unsigned char b[4] = {(unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF),
                          (unsigned char)((v >> 16) & 0xFF), (unsigned char)((v >> 24) & 0xFF)};
    fwrite(b, 1, 4, f);
}

static void dos_datetime(unsigned *dtime, unsigned *ddate)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (!tm) { *dtime = 0; *ddate = (1 << 5) | 1; return; }
    *dtime = (unsigned)((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
    {
        int year = tm->tm_year + 1900;
        if (year < 1980) year = 1980;
        *ddate = (unsigned)(((year - 1980) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
    }
}

int zip_open(zip_writer *z, const char *path)
{
    memset(z, 0, sizeof(*z));
    z->f = fopen(path, "wb");
    return z->f != NULL;
}

int zip_add(zip_writer *z, const char *name, const void *data, size_t len)
{
    unsigned dtime, ddate;
    zip_entry e;
    if (!z->f) return 0;
    dos_datetime(&dtime, &ddate);
    e.name = (char *)malloc(strlen(name) + 1);
    strcpy(e.name, name);
    e.crc = zip_crc32(data, len);
    e.size = (unsigned)len;
    e.offset = (unsigned)ftell(z->f);
    put32(z->f, 0x04034b50u);
    put16(z->f, 20);            /* version needed */
    put16(z->f, 0);             /* flags */
    put16(z->f, 0);             /* method: stored */
    put16(z->f, dtime);
    put16(z->f, ddate);
    put32(z->f, e.crc);
    put32(z->f, e.size);
    put32(z->f, e.size);
    put16(z->f, (unsigned)strlen(name));
    put16(z->f, 0);             /* extra length */
    fwrite(name, 1, strlen(name), z->f);
    if (len) fwrite(data, 1, len, z->f);
    z->entries = (zip_entry *)realloc(z->entries, sizeof(zip_entry) * (z->nentries + 1));
    z->entries[z->nentries++] = e;
    return !ferror(z->f);
}

int zip_close(zip_writer *z)
{
    unsigned cd_start, cd_size, dtime, ddate;
    int i, ok;
    if (!z->f) return 0;
    dos_datetime(&dtime, &ddate);
    cd_start = (unsigned)ftell(z->f);
    for (i = 0; i < z->nentries; i++) {
        const zip_entry *e = &z->entries[i];
        put32(z->f, 0x02014b50u);
        put16(z->f, 20);        /* version made by */
        put16(z->f, 20);        /* version needed */
        put16(z->f, 0);
        put16(z->f, 0);
        put16(z->f, dtime);
        put16(z->f, ddate);
        put32(z->f, e->crc);
        put32(z->f, e->size);
        put32(z->f, e->size);
        put16(z->f, (unsigned)strlen(e->name));
        put16(z->f, 0);         /* extra */
        put16(z->f, 0);         /* comment */
        put16(z->f, 0);         /* disk */
        put16(z->f, 0);         /* internal attrs */
        put32(z->f, 0);         /* external attrs */
        put32(z->f, e->offset);
        fwrite(e->name, 1, strlen(e->name), z->f);
    }
    cd_size = (unsigned)ftell(z->f) - cd_start;
    put32(z->f, 0x06054b50u);
    put16(z->f, 0);
    put16(z->f, 0);
    put16(z->f, (unsigned)z->nentries);
    put16(z->f, (unsigned)z->nentries);
    put32(z->f, cd_size);
    put32(z->f, cd_start);
    put16(z->f, 0);
    ok = !ferror(z->f);
    if (fclose(z->f) != 0) ok = 0;
    for (i = 0; i < z->nentries; i++) free(z->entries[i].name);
    free(z->entries);
    memset(z, 0, sizeof(*z));
    return ok;
}
