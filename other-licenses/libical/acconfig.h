/* Define to make icalerror_* calls abort instead of internally
   signalling an error */
#undef ICAL_ERRORS_ARE_FATAL

/* Define to make icalcluster_commit() save to a temp file and mv to
   the original file instead of writing to the orig file directly */
#undef ICAL_SAFESAVES

/* Define to terminate lines with "\n" instead of "\r\n" */
#undef ICAL_UNIX_NEWLINE

/* Define if your libc defines a "timezone" variable */
#undef HAVE_TIMEZONE
 
/* Define if your libc defines a struct tm containing a "tm_gmtoff" member */  
#undef HAVE_TM_GMTOFF

/* Define to 1 if your compile does not like lines like: struct
   something foo[]*/
#undef ICAL_NO_EMPTY_ARRAY_DECL

/* Define to 1 if you DO NOT WANT to see deprecated messages */
#undef NO_WARN_DEPRECATED

/* Define to 1 if you DO NO WANT to see the warning messages 
related to ICAL_MALFORMEDDATA_ERROR and parsing .ics zoneinfo files */
#undef NO_WARN_ICAL_MALFORMEDDATA_ERROR_HACK
