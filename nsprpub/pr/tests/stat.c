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

/*
 * Program to test different ways to get file info; right now it 
 * only works for solaris and OS/2.
 *
 */
#include "nspr.h"
#include "prpriv.h"
#include "prinrval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_OS2
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#define DEFAULT_COUNT 100000
PRInt32 count;

#ifndef XP_PC
char *filename = "/etc/passwd";
#else
char *filename = "..\\stat.c";
#endif

static void statPRStat(void)
{
    PRFileInfo finfo;
    PRInt32 index = count;
 
    for (;index--;) {
         PR_GetFileInfo(filename, &finfo);
    }
}

static void statStat(void)
{
    struct stat finfo;
    PRInt32 index = count;
 
    for (;index--;) {
        stat(filename, &finfo);
    }
}

/************************************************************************/

static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;
    PRInt32 tot;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);
    tot = PR_IntervalToMilliseconds(stop-start);

    printf("%40s: %6.2f usec avg, %d msec total\n", msg, d / count, tot);
}

void main(int argc, char **argv)
{
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    if (argc > 1) {
	count = atoi(argv[1]);
    } else {
	count = DEFAULT_COUNT;
    }

    Measure(statPRStat, "time to call PR_GetFileInfo()");
    Measure(statStat, "time to call stat()");

    PR_Cleanup();
}
