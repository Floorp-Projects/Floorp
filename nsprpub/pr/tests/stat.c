/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
