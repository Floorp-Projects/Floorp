/*
 * A generic debugging printer.
 *
 * Conrad Parker <conrad@metadecks.org>, May 2009
 *
 * Usage:
 *
 * #define DEBUG_LEVEL 3
 * #include "debug.h"
 *
 * ...
 *     debug_printf (2, "Something went wrong");
 * ...
 *
 * The macro print_debug(level, fmt) prints a formatted debugging message
 * of level 'level' to stderr. You should set the maximum tolerable debug
 * level before including debug.h. If it is 0, or if neither DEBUG_LEVEL
 * nor DEBUG are defined, then the debug_printf() macro is ignored, and
 * none of this file is included.
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

/* MSVC can't handle C99 */
#if (defined (_MSCVER) || defined (_MSC_VER))
#define debug_printf //
#else

#ifdef DEBUG
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1
#endif
#endif

#if (DEBUG_LEVEL > 0)

#define DEBUG_MAXLINE 4096

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/*
 * debug_print_err (func, line, fmt)
 *
 * Print a formatted error message to stderr.
 */
static void
debug_print_err (const char * func, int line, const char * fmt, ...)
{
  va_list ap;
  int errno_save;
  char buf[DEBUG_MAXLINE];
  int n=0;

  errno_save = errno;

  va_start (ap, fmt);

  if (func) {
    snprintf (buf+n, DEBUG_MAXLINE-n, "%s():%d: ", func, line);
    n = strlen (buf);
  }

  vsnprintf (buf+n, DEBUG_MAXLINE-n, fmt, ap);
  n = strlen (buf);

  fflush (stdout); /* in case stdout and stderr are the same */
  fputs (buf, stderr);
  fputc ('\n', stderr);
  fflush (NULL);

  va_end (ap);
}

/*
 * debug_printf (level, fmt)
 *
 * Print a formatted debugging message of level 'level' to stderr
 */
#define debug_printf(x,y...) {if (x <= DEBUG_LEVEL) debug_print_err (__func__, __LINE__, y);}

#undef MAXLINE

#else
#define debug_printf(x,y...)
#endif

#endif /* non-C99 */

#endif /* __DEBUG_H__ */
