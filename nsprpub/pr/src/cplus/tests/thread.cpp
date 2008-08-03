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

/* thread.cpp - a test program */

#include "rcthread.h"

#include <prlog.h>

#include <stdio.h>

class TestThread: public RCThread
{
public:
    TestThread(RCThread::State state, PRIntn count);

    virtual void RootFunction();

protected:
    virtual ~TestThread();

private:
    PRUint32 mydata;
};

TestThread::~TestThread() { }

TestThread::TestThread(RCThread::State state, PRIntn count):
    RCThread(RCThread::global, state, 0) { mydata = count; }

void TestThread::RootFunction()
{
    SetPriority(RCThread::high);
    printf("TestThread::RootFunction %d did it\n", mydata);
}  /* TestThread::RootFunction */

class Foo1
{
public:
    Foo1();
    virtual ~Foo1();

    TestThread *thread;
    PRIntn data;
};

Foo1::Foo1() 
{
    data = 0xafaf;
    thread = new TestThread(RCThread::joinable, 0xafaf);
    thread->Start();
}

Foo1::~Foo1()
{
    PRStatus rv = thread->Join();
    PR_ASSERT(PR_SUCCESS == rv);
}  /* Foo1::~Foo1 */

PRIntn main(PRIntn argc, char **agrv)
{
    PRStatus status;
    PRIntn count = 100;
    RCThread *thread[10];
    while (--count > 0)
    {
        TestThread *thread = new TestThread(RCThread::joinable, count);
        status = thread->Start();  /* have to remember to start it */
        PR_ASSERT(PR_SUCCESS == status);
        status = thread->Join();  /* this should work */
        PR_ASSERT(PR_SUCCESS == status);
    }
    while (++count < 100)
    {
        TestThread *thread = new TestThread(RCThread::unjoinable, count);
        status = thread->Start();  /* have to remember to start it */
        PR_ASSERT(PR_SUCCESS == status);
    }

    {
        Foo1 *foo1 = new Foo1();
        PR_ASSERT(NULL != foo1);
        delete foo1;
    }

    {
        for (count = 0; count < 10; ++count)
        {
            thread[count] = new TestThread( RCThread::joinable, count);
            status = thread[count]->Start();  /* have to remember to start it */
            PR_ASSERT(PR_SUCCESS == status);
        }
        for (count = 0; count < 10; ++count)
        {
            PRStatus rv = thread[count]->Join();
            PR_ASSERT(PR_SUCCESS == rv);
        }
    }

    (void)RCPrimordialThread::Cleanup();

    return 0;
}  /* main */

/* thread.cpp */

