/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 *			Mike Blakely
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */
/* calls for general error handling */

#include "bench.h"

#ifdef HAVE_VPRINTF
#define	VPRINTF(stderr, format, args)	vfprintf((stderr), (format), (args))
#else
#ifdef HAVE_DOPRNT
#define VPRINTF(stderr, format, args)	_doprnt((format), (args), (stderr))
#endif /* HAVE_DOPRNT */
#endif /* HAVE_VPRINTF */

/* print an error message and exit 1 */
void
errexit(FILE *dfile, const char *format, ...)
{
    time_t	t=time(0L) - gt_startedtime;
    va_list args;
#if defined (HAVE_SNPRINTF) && defined (HAVE_VPRINTF)
    char	buff[1024];
    int		r;
#endif

    va_start(args, format);

#if defined (HAVE_SNPRINTF) && defined (HAVE_VPRINTF)
    /* puts as one chunk so that output doesnt get mixed up */
    r = snprintf(buff, sizeof(buff),
	     "%s[%d]\tt=%lu EXITING: ", gs_thishostname, gn_myPID, t);
    vsnprintf(buff+r, sizeof(buff) - r, format, args);
    fputs (buff, stderr);
    fflush(stderr);

    if (gn_debug && dfile && (dfile != stderr)) {
	fputs (buff, dfile);
	fflush(dfile);
    }
#else
    fprintf(stderr,
	    "%s[%d]\tt=%lu EXITING: ", gs_thishostname, gn_myPID, t);
    VPRINTF(stderr, format, args);
    fflush(stderr);

    if (gn_debug && dfile && (dfile != stderr)) {
	fprintf(dfile,
		"%s[%d]\tt=%lu EXITING: ", gs_thishostname, gn_myPID, t);
	VPRINTF(dfile, format, args);
	fflush(dfile);
    }
#endif
    va_end(args);
    exit(1);
}

/* This is the main feedback path for errors and status */
/* print an error message and return -1 */
/* Also log to the debug file if available */
int
returnerr(FILE *dfile, const char *format, ...)
{
    time_t	t=time(0L) - gt_startedtime;
    va_list args;
#if defined (HAVE_SNPRINTF) && defined (HAVE_VPRINTF)
    char	buff[1024];
    int		r;
#endif

    va_start(args, format);

#if defined (HAVE_SNPRINTF) && defined (HAVE_VPRINTF)
    /* puts as one chunk so that output doesnt get mixed up */
    r = snprintf(buff, sizeof(buff),
	     "%s[%d]\tt=%lu: ", gs_thishostname, gn_myPID, t);
    vsnprintf(buff+r, sizeof(buff) - r, format, args);
    fputs (buff, stderr);
    fflush(stderr);

    if (gn_debug && dfile && (dfile != stderr)) {
	fputs (buff, dfile);
	fflush(dfile);
    }
#else
    fprintf(stderr,
	    "%s[%d]\tt=%lu: ", gs_thishostname, gn_myPID, t);
    VPRINTF(stderr, format, args);
    fflush(stderr);

    if (gn_debug && dfile && (dfile != stderr)) {
	fprintf(dfile,
		"%s[%d]\tt=%lu: ", gs_thishostname, gn_myPID, t);
	VPRINTF(dfile, format, args);
	fflush(dfile);
    }
#endif
    va_end(args);
    return(-1);
}

/* print a debug message and then flush */
int
d_printf(FILE *dfile, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    if (dfile) {
      fprintf(dfile, "%s: ", gs_thishostname);
      VPRINTF(dfile, format, args);
    }
    va_end(args);
    
    if (dfile) fflush(dfile);
    return 0;
}
/* that's it */

/* 
   Like d_printf, but for transaction logs.
 */
int
t_printf(int fd, const char *buffer, size_t count, const char *format, ...)
{
    time_t	t=time(0L) - gt_startedtime;
    va_list args;
    char	buff[1024];
    int	r;

    if (fd <= 0) return 0;

    va_start(args, format);
#if defined (HAVE_SNPRINTF) && defined (HAVE_VPRINTF)
    r = snprintf(buff, sizeof(buff),	/* stick in standard info */
		 "<LOG t=%lu length=%d ", t, count);
    snprintf(buff + r, sizeof(buff) - r, format, args);
    strcat (buff, ">\n");
    write (fd, buff, strlen(buff));

    if (count)
	write (fd, buffer, count);	/* write the (possibly binary) data */

    r = snprintf(buff, sizeof(buff),	/* terminate entry cleanly */
		 "\n</LOG t=%lu length=%d>\n", t, count);
    write (fd, buff, strlen(buff));
#else
    if (count)
	write (fd, buffer, count);	/* write the (possibly binary) data */
#endif
    va_end(args);
    
    return 0;
}
/* that's it */

/* returns the last network error as a string */
char *
neterrstr(void) {
    static char buf[200];

#ifdef _WIN32
    sprintf(buf, "WSAGetLastError()=%d", WSAGetLastError());
    WSASetLastError(0);
#else /* !_WIN32 */
    sprintf(buf, "errno=%d: %s", errno, strerror(errno));
    errno = 0;
#endif /* _WIN32 */
   
    return buf;
}
