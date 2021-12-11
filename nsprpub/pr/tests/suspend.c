/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    return 0;
}

