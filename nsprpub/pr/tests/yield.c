/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "prthread.h"
#include "prinit.h"
#ifndef XP_OS2
#include "private/pprmisc.h"
#include <windows.h>
#else
#include "primpl.h"
#include <os2.h>
#endif

#define THREADS 10


void
threadmain(void *_id)
{
    int id = (int)_id;
    int index;

    printf("thread %d alive\n", id);
    for (index=0; index<10; index++) {
        printf("thread %d yielding\n", id);
        PR_Sleep(0);
        printf("thread %d awake\n", id);
    }
    printf("thread %d dead\n", id);

}

int main(int argc, char **argv)
{
    int index;
    PRThread *a[THREADS];

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 5);
    PR_STDIO_INIT();

    for (index=0; index<THREADS; index++) {
        a[index] = PR_CreateThread(PR_USER_THREAD,
                                   threadmain,
                                   (void *)index,
                                   PR_PRIORITY_NORMAL,
                                   index%2?PR_LOCAL_THREAD:PR_GLOBAL_THREAD,
                                   PR_JOINABLE_THREAD,
                                   0);
    }
    for(index=0; index<THREADS; index++) {
        PR_JoinThread(a[index]);
    }
    printf("main dying\n");
}
