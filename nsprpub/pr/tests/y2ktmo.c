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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
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
 * Test: y2ktmo
 *
 * Description:
 *   This test tests the interval time facilities in NSPR for Y2K
 *   compliance.  All the functions that take a timeout argument
 *   are tested: PR_Sleep, socket I/O (PR_Accept is taken as a
 *   representative), PR_Poll, PR_WaitCondVar, PR_Wait, and
 *   PR_CWait.  A thread of each thread scope (local, global, and
 *   global bound) is created to call each of these functions.
 *   The test should be started at the specified number of seconds
 *   (called the lead time) before a Y2K rollover test date.  The
 *   timeout values for these threads will span over the rollover
 *   date by at least the specified number of seconds.  For
 *   example, if the lead time is 5 seconds, the test should
 *   be started at time (D - 5), where D is a rollover date, and
 *   the threads will time out at or after time (D + 5).  The
 *   timeout values for the threads are spaced one second apart.
 *
 *   When a thread times out, it calls PR_IntervalNow() to verify
 *   that it did wait for the specified time.  In addition, it
 *   calls a platform-native function to verify the actual elapsed
 *   time again, to rule out the possibility that PR_IntervalNow()
 *   is broken.  We allow the actual elapsed time to deviate from
 *   the specified timeout by a certain tolerance (in milliseconds).
 */ 

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(XP_UNIX)
#include <sys/time.h> /* for gettimeofday */
#endif
#if defined(WIN32)
#include <sys/types.h>
#include <sys/timeb.h>  /* for _ftime */
#endif

#define DEFAULT_LEAD_TIME_SECS 5
#define DEFAULT_TOLERANCE_MSECS 500

static PRBool debug_mode = PR_FALSE;
static PRInt32 lead_time_secs = DEFAULT_LEAD_TIME_SECS;
static PRInt32 tolerance_msecs = DEFAULT_TOLERANCE_MSECS;
static PRIntervalTime start_time;
static PRIntervalTime tolerance;

#if defined(XP_UNIX)
static struct timeval start_time_tv;
#endif
#if defined(WIN32)
static struct _timeb start_time_tb;
#endif

static void SleepThread(void *arg)
{
    PRIntervalTime timeout = (PRIntervalTime) arg;
    PRIntervalTime elapsed;
#if defined(XP_UNIX) || defined(WIN32)
    PRInt32 timeout_msecs = PR_IntervalToMilliseconds(timeout);
    PRInt32 elapsed_msecs;
#endif
#if defined(XP_UNIX)
    struct timeval end_time_tv;
#endif
#if defined(WIN32)
    struct _timeb end_time_tb;
#endif

    if (PR_Sleep(timeout) == PR_FAILURE) {
        fprintf(stderr, "PR_Sleep failed\n");
        exit(1);
    }
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start_time);
    if (elapsed + tolerance < timeout || elapsed > timeout + tolerance) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#if defined(XP_UNIX)
    gettimeofday(&end_time_tv, NULL);
    elapsed_msecs = 1000*(end_time_tv.tv_sec - start_time_tv.tv_sec)
            + (end_time_tv.tv_usec - start_time_tv.tv_usec)/1000;
#endif
#if defined(WIN32)
    _ftime(&end_time_tb);
    elapsed_msecs = 1000*(end_time_tb.time - start_time_tb.time)
            + (end_time_tb.millitm - start_time_tb.millitm);
#endif
#if defined(XP_UNIX) || defined(WIN32)
    if (elapsed_msecs + tolerance_msecs < timeout_msecs
            || elapsed_msecs > timeout_msecs + tolerance_msecs) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#endif
    if (debug_mode) {
        fprintf(stderr, "Sleep thread (scope %d) done\n",
                PR_GetThreadScope(PR_GetCurrentThread()));
    }
}

static void AcceptThread(void *arg)
{
    PRIntervalTime timeout = (PRIntervalTime) arg;
    PRIntervalTime elapsed;
#if defined(XP_UNIX) || defined(WIN32)
    PRInt32 timeout_msecs = PR_IntervalToMilliseconds(timeout);
    PRInt32 elapsed_msecs;
#endif
#if defined(XP_UNIX)
    struct timeval end_time_tv;
#endif
#if defined(WIN32)
    struct _timeb end_time_tb;
#endif
    PRFileDesc *sock;
    PRNetAddr addr;
    PRFileDesc *accepted;

    sock = PR_NewTCPSocket();
    if (sock == NULL) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.inet.family = PR_AF_INET;
    addr.inet.port = 0;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    if (PR_Bind(sock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    if (PR_Listen(sock, 5) == PR_FAILURE) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }
    accepted = PR_Accept(sock, NULL, timeout);
    if (accepted != NULL || PR_GetError() != PR_IO_TIMEOUT_ERROR) {
        fprintf(stderr, "PR_Accept did not time out\n");
        exit(1);
    }
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start_time);
    if (elapsed + tolerance < timeout || elapsed > timeout + tolerance) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#if defined(XP_UNIX)
    gettimeofday(&end_time_tv, NULL);
    elapsed_msecs = 1000*(end_time_tv.tv_sec - start_time_tv.tv_sec)
            + (end_time_tv.tv_usec - start_time_tv.tv_usec)/1000;
#endif
#if defined(WIN32)
    _ftime(&end_time_tb);
    elapsed_msecs = 1000*(end_time_tb.time - start_time_tb.time)
            + (end_time_tb.millitm - start_time_tb.millitm);
#endif
#if defined(XP_UNIX) || defined(WIN32)
    if (elapsed_msecs + tolerance_msecs < timeout_msecs
            || elapsed_msecs > timeout_msecs + tolerance_msecs) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#endif
    if (PR_Close(sock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    if (debug_mode) {
        fprintf(stderr, "Accept thread (scope %d) done\n",
                PR_GetThreadScope(PR_GetCurrentThread()));
    }
}

static void PollThread(void *arg)
{
    PRIntervalTime timeout = (PRIntervalTime) arg;
    PRIntervalTime elapsed;
#if defined(XP_UNIX) || defined(WIN32)
    PRInt32 timeout_msecs = PR_IntervalToMilliseconds(timeout);
    PRInt32 elapsed_msecs;
#endif
#if defined(XP_UNIX)
    struct timeval end_time_tv;
#endif
#if defined(WIN32)
    struct _timeb end_time_tb;
#endif
    PRFileDesc *sock;
    PRNetAddr addr;
    PRPollDesc pd;
    PRIntn rv;

    sock = PR_NewTCPSocket();
    if (sock == NULL) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.inet.family = PR_AF_INET;
    addr.inet.port = 0;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    if (PR_Bind(sock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    if (PR_Listen(sock, 5) == PR_FAILURE) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }
    pd.fd = sock;
    pd.in_flags = PR_POLL_READ;
    rv = PR_Poll(&pd, 1, timeout);
    if (rv != 0) {
        fprintf(stderr, "PR_Poll did not time out\n");
        exit(1);
    }
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start_time);
    if (elapsed + tolerance < timeout || elapsed > timeout + tolerance) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#if defined(XP_UNIX)
    gettimeofday(&end_time_tv, NULL);
    elapsed_msecs = 1000*(end_time_tv.tv_sec - start_time_tv.tv_sec)
            + (end_time_tv.tv_usec - start_time_tv.tv_usec)/1000;
#endif
#if defined(WIN32)
    _ftime(&end_time_tb);
    elapsed_msecs = 1000*(end_time_tb.time - start_time_tb.time)
            + (end_time_tb.millitm - start_time_tb.millitm);
#endif
#if defined(XP_UNIX) || defined(WIN32)
    if (elapsed_msecs + tolerance_msecs < timeout_msecs
            || elapsed_msecs > timeout_msecs + tolerance_msecs) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#endif
    if (PR_Close(sock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    if (debug_mode) {
        fprintf(stderr, "Poll thread (scope %d) done\n",
                PR_GetThreadScope(PR_GetCurrentThread()));
    }
}

static void WaitCondVarThread(void *arg)
{
    PRIntervalTime timeout = (PRIntervalTime) arg;
    PRIntervalTime elapsed;
#if defined(XP_UNIX) || defined(WIN32)
    PRInt32 timeout_msecs = PR_IntervalToMilliseconds(timeout);
    PRInt32 elapsed_msecs;
#endif
#if defined(XP_UNIX)
    struct timeval end_time_tv;
#endif
#if defined(WIN32)
    struct _timeb end_time_tb;
#endif
    PRLock *ml;
    PRCondVar *cv;

    ml = PR_NewLock();
    if (ml == NULL) {
        fprintf(stderr, "PR_NewLock failed\n");
        exit(1);
    }
    cv = PR_NewCondVar(ml);
    if (cv == NULL) {
        fprintf(stderr, "PR_NewCondVar failed\n");
        exit(1);
    }
    PR_Lock(ml);
    PR_WaitCondVar(cv, timeout);
    PR_Unlock(ml);
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start_time);
    if (elapsed + tolerance < timeout || elapsed > timeout + tolerance) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#if defined(XP_UNIX)
    gettimeofday(&end_time_tv, NULL);
    elapsed_msecs = 1000*(end_time_tv.tv_sec - start_time_tv.tv_sec)
            + (end_time_tv.tv_usec - start_time_tv.tv_usec)/1000;
#endif
#if defined(WIN32)
    _ftime(&end_time_tb);
    elapsed_msecs = 1000*(end_time_tb.time - start_time_tb.time)
            + (end_time_tb.millitm - start_time_tb.millitm);
#endif
#if defined(XP_UNIX) || defined(WIN32)
    if (elapsed_msecs + tolerance_msecs < timeout_msecs
            || elapsed_msecs > timeout_msecs + tolerance_msecs) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#endif
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    if (debug_mode) {
        fprintf(stderr, "wait cond var thread (scope %d) done\n",
                PR_GetThreadScope(PR_GetCurrentThread()));
    }
}

static void WaitMonitorThread(void *arg)
{
    PRIntervalTime timeout = (PRIntervalTime) arg;
    PRIntervalTime elapsed;
#if defined(XP_UNIX) || defined(WIN32)
    PRInt32 timeout_msecs = PR_IntervalToMilliseconds(timeout);
    PRInt32 elapsed_msecs;
#endif
#if defined(XP_UNIX)
    struct timeval end_time_tv;
#endif
#if defined(WIN32)
    struct _timeb end_time_tb;
#endif
    PRMonitor *mon;

    mon = PR_NewMonitor();
    if (mon == NULL) {
        fprintf(stderr, "PR_NewMonitor failed\n");
        exit(1);
    }
    PR_EnterMonitor(mon);
    PR_Wait(mon, timeout);
    PR_ExitMonitor(mon);
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start_time);
    if (elapsed + tolerance < timeout || elapsed > timeout + tolerance) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#if defined(XP_UNIX)
    gettimeofday(&end_time_tv, NULL);
    elapsed_msecs = 1000*(end_time_tv.tv_sec - start_time_tv.tv_sec)
            + (end_time_tv.tv_usec - start_time_tv.tv_usec)/1000;
#endif
#if defined(WIN32)
    _ftime(&end_time_tb);
    elapsed_msecs = 1000*(end_time_tb.time - start_time_tb.time)
            + (end_time_tb.millitm - start_time_tb.millitm);
#endif
#if defined(XP_UNIX) || defined(WIN32)
    if (elapsed_msecs + tolerance_msecs < timeout_msecs
            || elapsed_msecs > timeout_msecs + tolerance_msecs) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#endif
    PR_DestroyMonitor(mon);
    if (debug_mode) {
        fprintf(stderr, "wait monitor thread (scope %d) done\n",
                PR_GetThreadScope(PR_GetCurrentThread()));
    }
}

static void WaitCMonitorThread(void *arg)
{
    PRIntervalTime timeout = (PRIntervalTime) arg;
    PRIntervalTime elapsed;
#if defined(XP_UNIX) || defined(WIN32)
    PRInt32 timeout_msecs = PR_IntervalToMilliseconds(timeout);
    PRInt32 elapsed_msecs;
#endif
#if defined(XP_UNIX)
    struct timeval end_time_tv;
#endif
#if defined(WIN32)
    struct _timeb end_time_tb;
#endif
    int dummy;

    PR_CEnterMonitor(&dummy);
    PR_CWait(&dummy, timeout);
    PR_CExitMonitor(&dummy);
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start_time);
    if (elapsed + tolerance < timeout || elapsed > timeout + tolerance) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#if defined(XP_UNIX)
    gettimeofday(&end_time_tv, NULL);
    elapsed_msecs = 1000*(end_time_tv.tv_sec - start_time_tv.tv_sec)
            + (end_time_tv.tv_usec - start_time_tv.tv_usec)/1000;
#endif
#if defined(WIN32)
    _ftime(&end_time_tb);
    elapsed_msecs = 1000*(end_time_tb.time - start_time_tb.time)
            + (end_time_tb.millitm - start_time_tb.millitm);
#endif
#if defined(XP_UNIX) || defined(WIN32)
    if (elapsed_msecs + tolerance_msecs < timeout_msecs
            || elapsed_msecs > timeout_msecs + tolerance_msecs) {
        fprintf(stderr, "timeout wrong\n");
        exit(1);
    }
#endif
    if (debug_mode) {
        fprintf(stderr, "wait cached monitor thread (scope %d) done\n",
                PR_GetThreadScope(PR_GetCurrentThread()));
    }
}

typedef void (*NSPRThreadFunc)(void*);

static NSPRThreadFunc threadFuncs[] = {
    SleepThread, AcceptThread, PollThread,
    WaitCondVarThread, WaitMonitorThread, WaitCMonitorThread};

static PRThreadScope threadScopes[] = {
    PR_LOCAL_THREAD, PR_GLOBAL_THREAD, PR_GLOBAL_BOUND_THREAD};

static void Help(void)
{
    fprintf(stderr, "y2ktmo test program usage:\n");
    fprintf(stderr, "\t-d           debug mode         (FALSE)\n");
    fprintf(stderr, "\t-l <secs>    lead time          (%d)\n",
            DEFAULT_LEAD_TIME_SECS);
    fprintf(stderr, "\t-t <msecs>   tolerance          (%d)\n",
            DEFAULT_TOLERANCE_MSECS);
    fprintf(stderr, "\t-h           this message\n");
}  /* Help */

int main(int argc, char **argv)
{
    PRThread **threads;
    int num_thread_funcs = sizeof(threadFuncs)/sizeof(NSPRThreadFunc);
    int num_thread_scopes = sizeof(threadScopes)/sizeof(PRThreadScope);
    int i, j;
    int idx;
    PRInt32 secs;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dl:t:h");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option) {
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'l':  /* lead time */
                lead_time_secs = atoi(opt->value);
                break;
            case 't':  /* tolerance */
                tolerance_msecs = atoi(opt->value);
                break;
            case 'h':
            default:
                Help();
                return 2;
        }
    }
    PL_DestroyOptState(opt);

    if (debug_mode) {
        fprintf(stderr, "lead time: %d secs\n", lead_time_secs);
        fprintf(stderr, "tolerance: %d msecs\n", tolerance_msecs);
    }

    start_time = PR_IntervalNow();
#if defined(XP_UNIX)
    gettimeofday(&start_time_tv, NULL);
#endif
#if defined(WIN32)
    _ftime(&start_time_tb);
#endif
    tolerance = PR_MillisecondsToInterval(tolerance_msecs);

    threads = PR_Malloc(
            num_thread_scopes * num_thread_funcs * sizeof(PRThread*));
    if (threads == NULL) {
        fprintf(stderr, "PR_Malloc failed\n");
        exit(1);
    }

    /* start to time out 5 seconds after a rollover date */
    secs = lead_time_secs + 5;
    idx = 0;
    for (i = 0; i < num_thread_scopes; i++) { 
        for (j = 0; j < num_thread_funcs; j++) {
            threads[idx] = PR_CreateThread(PR_USER_THREAD, threadFuncs[j],
                (void*)PR_SecondsToInterval(secs), PR_PRIORITY_NORMAL,
                threadScopes[i], PR_JOINABLE_THREAD, 0);
            if (threads[idx] == NULL) {
                fprintf(stderr, "PR_CreateThread failed\n");
                exit(1);
            }
            secs++;
            idx++;
        }
    }
    for (idx = 0; idx < num_thread_scopes*num_thread_funcs; idx++) {
        if (PR_JoinThread(threads[idx]) == PR_FAILURE) {
            fprintf(stderr, "PR_JoinThread failed\n");
            exit(1);
        }
    }
    PR_Free(threads);
    printf("PASS\n");
    return 0;
}
