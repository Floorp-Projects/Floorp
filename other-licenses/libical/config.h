/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if lex declares yytext as a char * by default, not a char[].  */
#define YYTEXT_POINTER 1

/* Define to make icalerror_* calls abort instead of internally
   signalling an error */
#define ICAL_ERRORS_ARE_FATAL 1

/* Define to make icalcluster_commit() save to a temp file and mv to
   the original file instead of writing to the orig file directly */
#define ICAL_SAFESAVES 1

/* Define to terminate lines with "\n" instead of "\r\n" */
/* #undef ICAL_UNIX_NEWLINE */

/* Define to 1 if you DO NOT WANT to see deprecated messages */
#define NO_WARN_DEPRECATED 1

/* Define to 1 if you DO NO WANT to see the warning messages 
related to ICAL_MALFORMEDDATA_ERROR and parsing .ics zoneinfo files */
#define NO_WARN_ICAL_MALFORMEDDATA_ERROR_HACK 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the <assert.h> header file.  */
#define HAVE_ASSERT_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

/* Name of package */
#define PACKAGE "libical"

/* Version number of package */
#define VERSION "0.23a"

