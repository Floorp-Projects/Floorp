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

/* RCThread.h */

#if defined(_RCTHREAD_H)
#else
#define _RCTHREAD_H

#include "rcbase.h"

#include <prthread.h>

class RCInterval;

class PR_IMPLEMENT(RCThreadPrivateData)
{
public:
    RCThreadPrivateData();
    RCThreadPrivateData(const RCThreadPrivateData&);

    virtual ~RCThreadPrivateData();

    virtual void Release() = 0;

};  /* RCThreadPrivateData */

class PR_IMPLEMENT(RCThread): public RCBase
{
public:

    typedef enum 
    {
        local = PR_LOCAL_THREAD, global = PR_GLOBAL_THREAD
    } Scope;

    typedef enum
    {
        joinable = PR_JOINABLE_THREAD, unjoinable = PR_UNJOINABLE_THREAD
    } State;

    typedef enum
    {
        first = PR_PRIORITY_FIRST,
        low = PR_PRIORITY_LOW,
        normal = PR_PRIORITY_NORMAL,
        high = PR_PRIORITY_HIGH,
        urgent = PR_PRIORITY_URGENT,
        last = PR_PRIORITY_LAST
    } Priority;

    /*
     * Create a new thread, providing scope and joinability state.
     */
    RCThread(Scope scope, State state, PRUint32 stackSize=0);

    /*
     * New threads are created in a suspended state. It must be 'started"
     * before it begins execution in the class' defined 'RootFunction()'.
     */
    virtual PRStatus Start();

    /*
     * If a thread is created joinable, then the thread's object exists
     * until join is called. The thread that calls join will block until
     * the target thread returns from it's root function.
     */
    virtual PRStatus Join();
    
    /*
     * The priority of a newly created thread is the same as the creator.
     * The priority may be changed either by the new thread itself, by
     * the creator or any other arbitrary thread.
     */   
    virtual void SetPriority(Priority newPriority);


    /*
     * Interrupt another thread, causing it to stop what it
     * is doing and return with a well known error code.
     */
    virtual PRStatus Interrupt();
    
    /*
     * And in case a thread was interrupted and didn't get a chance
     * to have the notification delivered, a way to cancel the pending
     * status.
     */
    static void ClearInterrupt();
    
    /*
     * Methods to discover the attributes of an existing thread.
     */
    static PRThread *Self();
    Scope GetScope() const;
    State GetState() const;
    Priority GetPriority() const;

    /*
     * Thread private data
     */
    static PRStatus NewPrivateIndex(PRUintn* index);

    /*
     * Getting it - if you want to modify, make a copy
     */
    static RCThreadPrivateData* GetPrivateData(PRUintn index);

    /*
     * Setting it to <empty> - deletes existing data
     */
    static PRStatus SetPrivateData(PRUintn index);

    /*
     * Setting it - runtime will make a copy, freeing old iff necessary
     */
    static PRStatus SetPrivateData(PRUintn index, RCThreadPrivateData* data);

    /*
     * Scheduling control
     */
    static PRStatus Sleep(const RCInterval& ticks);

    friend void nas_Root(void*);
    friend class RCPrimordialThread;
protected:

    /*
     * The instantiator of a class must not call the destructor. The base
     * implementation of Join will, and if the thread is created unjoinable,
     * then the code that called the RootFunction will call the desctructor.
     */
    virtual ~RCThread();

private:

    /*
     * This is where a newly created thread begins execution. Returning
     * from this function is equivalent to terminating the thread.
     */
    virtual void RootFunction() = 0;

    PRThread *identity;

    /* Threads are unstarted until started - pretty startling */
    enum {ex_unstarted, ex_started} execution;

    /* There is no public default constructor or copy constructor */
    RCThread();
    RCThread(const RCThread&);
    
    /* And there is no assignment operator */
    void operator=(const RCThread&);

public:
    static RCPrimordialThread *WrapPrimordialThread();    

 };
 
/*
** class RCPrimordialThread
*/
class PR_IMPLEMENT(RCPrimordialThread): public RCThread
{
public:
    /*
    ** The primordial thread can (optionally) wait for all created
    ** threads to terminate before allowing the process to exit.
    ** Not calling Cleanup() before returning from main() will cause
    ** the immediate termination of the entire process, including
    ** any running threads.
    */
    static PRStatus Cleanup();

    /*
    ** Only the primordial thread is allowed to adjust the number of
    ** virtual processors of the runtime. It's a lame security thing.
    */
    static PRStatus SetVirtualProcessors(PRIntn count=10);

friend class RCThread;
private:
    /*
    ** None other than the runtime can create of destruct
    ** a primordial thread. It is fabricated by the runtime
    ** to wrap the thread that initiated the application.
    */
    RCPrimordialThread();
    ~RCPrimordialThread();
    void RootFunction();
};  /* RCPrimordialThread */

 #endif /* defined(_RCTHREAD_H) */
