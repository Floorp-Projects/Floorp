#ifndef HAVE_SNPRINTF

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>

#include "prtypes.h"

#include <ncompat.h>

#include <stdarg.h>

int
snprintf(char *str, size_t n, const char *fmt, ...)
{
    va_list ap;
#ifdef VSPRINTF_CHARSTAR
    char *rp;
#else
    int rval;
#endif
    va_start(ap, fmt);
#ifdef VSPRINTF_CHARSTAR
    rp = vsprintf(str, fmt, ap);
    va_end(ap);
    return (strlen(rp));
#else
    rval = vsprintf(str, fmt, ap);
    va_end(ap);
    return (rval);
#endif
}

int
    vsnprintf(str, n, fmt, ap) char *str;
size_t n;
const char *fmt;
va_list ap;
{
#ifdef VSPRINTF_CHARSTAR
    return (strlen(vsprintf(str, fmt, ap)));
#else
    return (vsprintf(str, fmt, ap));
#endif
}

#endif /* HAVE_SNPRINTF */

/* Some compilers don't like an empty source file. */
static int dummy = 0;
