/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

  Some tests for nsAutoLock.

 */

#include "nsAutoLock.h"
#include "prthread.h"

PRLock* gLock;
int gCount;

static void run(void* arg)
{
    for (int i = 0; i < 1000000; ++i) {
        nsAutoLock guard(gLock);
        ++gCount;
        PR_ASSERT(gCount == 1);
        --gCount;
    }
}


int main(int argc, char** argv)
{
    gLock = PR_NewLock();
    gCount = 0;

    // This shouldn't compile
    //nsAutoLock* l1 = new nsAutoLock(theLock);
    //delete l1;

    // Create a block-scoped lock. This should compile.
    {
        nsAutoLock l2(gLock);
    }

    // Fork a thread to access the shared variable in a tight loop
    PRThread* t1 =
        PR_CreateThread(PR_SYSTEM_THREAD,
                        run,
                        nsnull,
                        PR_PRIORITY_NORMAL,
                        PR_GLOBAL_THREAD,
                        PR_JOINABLE_THREAD,
                        0);

    // ...and now do the same thing ourselves
    run(nsnull);

    // Wait for the background thread to finish, if necessary.
    PR_JoinThread(t1);
    return 0;
}
