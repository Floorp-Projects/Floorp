/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsIThread_h__
#define nsIThread_h__

#include "nsISupports.h"
#include "prthread.h"
#include "nscore.h"

////////////////////////////////////////////////////////////////////////////////

#define NS_IRUNNABLE_IID                             \
{ /* e06996e0-0d6b-11d3-9331-00104ba0fd40 */         \
    0xe06996e0,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIRunnable : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRUNNABLE_IID);

    NS_IMETHOD Run() = 0;

};

////////////////////////////////////////////////////////////////////////////////

#define NS_ITHREAD_IID                               \
{ /* ecbb89e0-0d6b-11d3-9331-00104ba0fd40 */         \
    0xecbb89e0,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIThread : public nsISupports 
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITHREAD_IID);

    // returns the nsIThread for the current thread:
    static NS_COM nsresult GetCurrent(nsIThread* *result);

    // returns the nsIThread for an arbitrary PRThread:
    static NS_COM nsresult GetIThread(PRThread* prthread, nsIThread* *result);

    NS_IMETHOD Join() = 0;

    NS_IMETHOD GetPriority(PRThreadPriority *result) = 0;
    NS_IMETHOD SetPriority(PRThreadPriority value) = 0;

    NS_IMETHOD Interrupt() = 0;

    NS_IMETHOD GetScope(PRThreadScope *result) = 0;
    NS_IMETHOD GetState(PRThreadState *result) = 0;

    NS_IMETHOD GetPRThread(PRThread* *result) = 0;
};

extern NS_COM nsresult
NS_NewThread(nsIThread* *result, 
             nsIRunnable* runnable,
             PRUint32 stackSize = 0,
             PRThreadPriority priority = PR_PRIORITY_NORMAL,
             PRThreadScope scope = PR_GLOBAL_THREAD,
             PRThreadState state = PR_JOINABLE_THREAD);

////////////////////////////////////////////////////////////////////////////////

#define NS_ITHREADPOOL_IID                           \
{ /* 074d0170-0d6c-11d3-9331-00104ba0fd40 */         \
    0x074d0170,                                      \
    0x0d6c,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

/**
 * A thread pool is used to create a group of worker threads that
 * share in the servicing of requests. Requests are run in the 
 * context of the worker thread.
 */
class nsIThreadPool : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITHREADPOOL_IID);

    NS_IMETHOD DispatchRequest(nsIRunnable* runnable) = 0;

    NS_IMETHOD ProcessPendingRequests() = 0;

    NS_IMETHOD Shutdown() = 0;
};

extern NS_COM nsresult
NS_NewThreadPool(nsIThreadPool* *result,
                 PRUint32 minThreads, PRUint32 maxThreads,
                 PRUint32 stackSize = 0,
                 PRThreadPriority priority = PR_PRIORITY_NORMAL,
                 PRThreadScope scope = PR_GLOBAL_THREAD);

////////////////////////////////////////////////////////////////////////////////

#endif // nsIThread_h__
