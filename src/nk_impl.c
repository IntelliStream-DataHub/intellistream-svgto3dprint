/* Nuklear implementation translation unit.
 * Number <-> string conversion goes through the C library, because Nuklear's
 * own routines print 0.6 as 0.59. */
#include <stdio.h>
#include <stdlib.h>

static char *l3d_dtoa(char *buf, double n)
{
    snprintf(buf, 64, "%.5g", n);
    return buf;
}

static double l3d_strtod(const char *s, const char **endptr)
{
    char *e;
    double v = strtod(s, &e);
    if (endptr) *endptr = e;
    return v;
}

#define NK_DTOA l3d_dtoa
#define NK_STRTOD l3d_strtod
#define NK_IMPLEMENTATION
#include "nk_config.h"
