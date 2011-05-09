/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_ReentrantMonitor_h
#define mozilla_ReentrantMonitor_h

#include "prmon.h"

#include "mozilla/BlockingResourceBase.h"

//
// Provides:
//
//  - ReentrantMonitor, a Java-like monitor
//  - ReentrantMonitorAutoEnter, an RAII class for ensuring that
//    ReentrantMonitors are properly entered and exited
//
// Using ReentrantMonitorAutoEnter is MUCH preferred to making bare calls to 
// ReentrantMonitor.Enter and Exit.
//
namespace mozilla {


/**
 * ReentrantMonitor
 * Java-like monitor.
 * When possible, use ReentrantMonitorAutoEnter to hold this monitor within a
 * scope, instead of calling Enter/Exit directly.
 **/
class NS_COM_GLUE ReentrantMonitor : BlockingResourceBase
{
public:
    /**
     * ReentrantMonitor
     * @param aName A name which can reference this monitor
     */
    ReentrantMonitor(const char* aName) :
        BlockingResourceBase(aName, eReentrantMonitor)
#ifdef DEBUG
        , mEntryCount(0)
#endif
    {
        MOZ_COUNT_CTOR(ReentrantMonitor);
        mReentrantMonitor = PR_NewMonitor();
        if (!mReentrantMonitor)
            NS_RUNTIMEABORT("Can't allocate mozilla::ReentrantMonitor");
    }

    /**
     * ~ReentrantMonitor
     **/
    ~ReentrantMonitor()
    {
        NS_ASSERTION(mReentrantMonitor,
                     "improperly constructed ReentrantMonitor or double free");
        PR_DestroyMonitor(mReentrantMonitor);
        mReentrantMonitor = 0;
        MOZ_COUNT_DTOR(ReentrantMonitor);
    }

#ifndef DEBUG
    /** 
     * Enter
     * @see prmon.h 
     **/
    void Enter()
    {
        PR_EnterMonitor(mReentrantMonitor);
    }

    /** 
     * Exit
     * @see prmon.h 
     **/
    void Exit()
    {
        PR_ExitMonitor(mReentrantMonitor);
    }

    /**
     * Wait
     * @see prmon.h
     **/      
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT)
    {
        return PR_Wait(mReentrantMonitor, interval) == PR_SUCCESS ?
            NS_OK : NS_ERROR_FAILURE;
    }

#else
    void Enter();
    void Exit();
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT);

#endif  // ifndef DEBUG

    /** 
     * Notify
     * @see prmon.h 
     **/      
    nsresult Notify()
    {
        return PR_Notify(mReentrantMonitor) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

    /** 
     * NotifyAll
     * @see prmon.h 
     **/      
    nsresult NotifyAll()
    {
        return PR_NotifyAll(mReentrantMonitor) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    /**
     * AssertCurrentThreadIn
     * @see prmon.h
     **/
    void AssertCurrentThreadIn()
    {
        PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mReentrantMonitor);
    }

    /**
     * AssertNotCurrentThreadIn
     * @see prmon.h
     **/
    void AssertNotCurrentThreadIn()
    {
        // FIXME bug 476536
    }

#else
    void AssertCurrentThreadIn()
    {
    }
    void AssertNotCurrentThreadIn()
    {
    }

#endif  // ifdef DEBUG

private:
    ReentrantMonitor();
    ReentrantMonitor(const ReentrantMonitor&);
    ReentrantMonitor& operator =(const ReentrantMonitor&);

    PRMonitor* mReentrantMonitor;
#ifdef DEBUG
    PRInt32 mEntryCount;
#endif
};


/**
 * ReentrantMonitorAutoEnter
 * Enters the ReentrantMonitor when it enters scope, and exits it when
 * it leaves scope.
 *
 * MUCH PREFERRED to bare calls to ReentrantMonitor.Enter and Exit.
 */ 
class NS_COM_GLUE NS_STACK_CLASS ReentrantMonitorAutoEnter
{
public:
    /**
     * Constructor
     * The constructor aquires the given lock.  The destructor
     * releases the lock.
     * 
     * @param aReentrantMonitor A valid mozilla::ReentrantMonitor*. 
     **/
    ReentrantMonitorAutoEnter(mozilla::ReentrantMonitor &aReentrantMonitor) :
        mReentrantMonitor(&aReentrantMonitor)
    {
        NS_ASSERTION(mReentrantMonitor, "null monitor");
        mReentrantMonitor->Enter();
    }
    
    ~ReentrantMonitorAutoEnter(void)
    {
        mReentrantMonitor->Exit();
    }
 
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT)
    {
       return mReentrantMonitor->Wait(interval);
    }

    nsresult Notify()
    {
        return mReentrantMonitor->Notify();
    }

    nsresult NotifyAll()
    {
        return mReentrantMonitor->NotifyAll();
    }

private:
    ReentrantMonitorAutoEnter();
    ReentrantMonitorAutoEnter(const ReentrantMonitorAutoEnter&);
    ReentrantMonitorAutoEnter& operator =(const ReentrantMonitorAutoEnter&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    mozilla::ReentrantMonitor* mReentrantMonitor;
};


} // namespace mozilla


#endif // ifndef mozilla_ReentrantMonitor_h
