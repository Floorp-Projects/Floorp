/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#if defined(XP_UNIX) || defined(XP_OS2_EMX) || defined(XP_BEOS)
#include <sys/time.h>
#elif defined(WIN32) || defined(XP_OS2_VACPP)
#include <sys/timeb.h>
#else
#error "Architecture not supported"
#endif


int main(int argc, char **argv)
{
#if defined(OMIT_LIB_BUILD_TIME)
    /*
     * Some platforms don't have any 64-bit integer type
     * such as 'long long'.  Because we can't use NSPR's
     * PR_snprintf in this program, it is difficult to
     * print a static initializer for PRInt64 (a struct).
     * So we print nothing.  The makefiles that build the
     * shared libraries will detect the empty output string
     * of this program and omit the library build time
     * in PRVersionDescription.
     */
#elif defined(XP_UNIX) || defined(XP_OS2_EMX) || defined(XP_BEOS)
    long long now;
    struct timeval tv;
#ifdef HAVE_SVID_GETTOD
    gettimeofday(&tv);
#else
    gettimeofday(&tv, NULL);
#endif
    now = ((1000000LL) * tv.tv_sec) + (long long)tv.tv_usec;
#if defined(OSF1)
    fprintf(stdout, "%ld", now);
#elif defined(BEOS) && defined(__POWERPC__)
    fprintf(stdout, "%Ld", now);  /* Metroworks on BeOS PPC */
#else
    fprintf(stdout, "%lld", now);
#endif

#elif defined(WIN32)
    __int64 now;
    struct timeb b;
    ftime(&b);
    now = b.time;
    now *= 1000000;
    now += (1000 * b.millitm);
    fprintf(stdout, "%I64d", now);

#elif defined(XP_OS2_VACPP)
/* no long long or i64 so we use a string */
#include <string.h>
  char buf[24];
  char tbuf[7];
  time_t now;
  long mtime;
  int i;

  struct timeb b;
  ftime(&b);
  now = b.time;
  _ltoa(now, buf, 10);

  mtime = b.millitm * 1000;
  if (mtime == 0){
    ++now;
    strcat(buf, "000000");
  } else {
     _ltoa(mtime, tbuf, 10);
     for (i = strlen(tbuf); i < 6; ++i)
       strcat(buf, "0");
     strcat(buf, tbuf);
  }
  fprintf(stdout, "%s", buf);

#else
#error "Architecture not supported"
#endif

    return 0;
}  /* main */

/* now.c */
