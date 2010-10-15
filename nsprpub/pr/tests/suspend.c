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

#ifdef XP_BEOS
#include <stdio.h>
int main()
{
    printf( "This test is not ported to the BeOS\n" );
    return 0;
}
#else

#include "nspr.h"
#include "prpriv.h"
#include "prinrval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PRMonitor *mon;
PRInt32 count;
PRInt32 alive;

#define SLEEP_TIME    4    /* secs */

void PR_CALLBACK
Level_2_Thread(void *arg)
{
    PR_Sleep(PR_MillisecondsToInterval(4 * 1000));
    printf("Level_2_Thread[0x%lx] exiting\n",PR_GetCurrentThread());
    return;
}

void PR_CALLBACK
Level_1_Thread(void *arg)
{
    PRUint32 tmp = (PRUint32)arg;
    PRThreadScope scope = (PRThreadScope) tmp;
    PRThread *thr;

    thr = PR_CreateThreadGCAble(PR_USER_THREAD,
        Level_2_Thread,
        NULL,
        PR_PRIORITY_HIGH,
        scope,
        PR_JOINABLE_THREAD,
        0);

    if (!thr) {
        printf("Could not create thread!\n");
    } else {
        printf("Level_1_Thread[0x%lx] created %15s thread 0x%lx\n",
            PR_GetCurrentThread(),
            (scope == PR_GLOBAL_THREAD) ?
            "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD",
            thr);
        PR_JoinThread(thr);
    }
    PR_EnterMonitor(mon);
    alive--;
    PR_Notify(mon);
    PR_ExitMonitor(mon);
    printf("Thread[0x%lx] exiting\n",PR_GetCurrentThread());
}

static PRStatus PR_CALLBACK print_thread(PRThread *thread, int i, void *arg)
{
    PRInt32 words;
    PRWord *registers;

    printf(
        "\nprint_thread[0x%lx]: %-20s - i = %ld\n",thread, 
        (PR_GLOBAL_THREAD == PR_GetThreadScope(thread)) ?
        "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD", i);
    registers = PR_GetGCRegisters(thread, 0, (int *)&words);
    if (registers)
        printf("Registers R0 = 0x%x R1 = 0x%x R2 = 0x%x R3 = 0x%x\n",
            registers[0],registers[1],registers[2],registers[3]);
    printf("Stack Pointer = 0x%lx\n", PR_GetSP(thread));
    return PR_SUCCESS;
}

static void Level_0_Thread(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *thr;
    PRThread *me = PR_GetCurrentThread();
    int n;
    PRInt32 words;
    PRWord *registers;

    alive = 0;
    mon = PR_NewMonitor();

    alive = count;
    for (n=0; n<count; n++) {
        thr = PR_CreateThreadGCAble(PR_USER_THREAD,
            Level_1_Thread, 
            (void *)scope2, 
            PR_PRIORITY_NORMAL,
            scope1,
            PR_UNJOINABLE_THREAD,
            0);
        if (!thr) {
            printf("Could not create thread!\n");
            alive--;
        }
        printf("Level_0_Thread[0x%lx] created %15s thread 0x%lx\n",
            PR_GetCurrentThread(),
            (scope1 == PR_GLOBAL_THREAD) ?
            "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD",
            thr);

        PR_Sleep(0);
    }
    PR_SuspendAll();
    PR_EnumerateThreads(print_thread, NULL);
    registers = PR_GetGCRegisters(me, 1, (int *)&words);
    if (registers)
        printf("My Registers: R0 = 0x%x R1 = 0x%x R2 = 0x%x R3 = 0x%x\n",
            registers[0],registers[1],registers[2],registers[3]);
    printf("My Stack Pointer = 0x%lx\n", PR_GetSP(me));
    PR_ResumeAll();

    /* Wait for all threads to exit */
    PR_EnterMonitor(mon);
    while (alive) {
        PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }

    PR_ExitMonitor(mon);
    PR_DestroyMonitor(mon);
}

static void CreateThreadsUU(void)
{
    Level_0_Thread(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
}

static void CreateThreadsUK(void)
{
    Level_0_Thread(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
}

static void CreateThreadsKU(void)
{
    Level_0_Thread(PR_GLOBAL_THREAD, PR_LOCAL_THREAD);
}

static void CreateThreadsKK(void)
{
    Level_0_Thread(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
}


int main(int argc, char **argv)
{
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    if (argc > 1) {
        count = atoi(argv[1]);
    } else {
        count = 5;
    }

    printf("\n\n%20s%30s\n\n"," ","Suspend_Resume Test");
    CreateThreadsUU();
    CreateThreadsUK();
    CreateThreadsKU();
    CreateThreadsKK();
    PR_SetConcurrency(2);

    printf("\n%20s%30s\n\n"," ","Added 2nd CPU\n");

    CreateThreadsUK();
    CreateThreadsKK();
    CreateThreadsUU();
    CreateThreadsKU();
    PR_Cleanup();
}

#endif /* XP_BEOS */
