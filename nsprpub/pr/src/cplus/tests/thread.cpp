/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

