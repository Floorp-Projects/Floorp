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
#include "prthread.h"
#include "prinit.h"

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

main()
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
    for(index=0; index<THREADS; index++)
        PR_JoinThread(a[index]);
    printf("main dying\n");
}
