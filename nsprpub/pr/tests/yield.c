/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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
