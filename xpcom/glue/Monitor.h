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

#ifndef mozilla_Monitor_h
#define mozilla_Monitor_h

#include "prmon.h"

#include "mozilla/BlockingResourceBase.h"

//
// Provides:
//
//  - Monitor, a Java-like monitor
//  - MonitorAutoEnter, an RAII class for ensuring that Monitors are properly 
//    entered and exited
//
// Using MonitorAutoEnter is MUCH preferred to making bare calls to 
// Monitor.Enter and Exit.
//
namespace mozilla {


/**
 * Monitor
 * Java-like monitor.
 * When possible, use MonitorAutoEnter to hold this monitor within a
 * scope, instead of calling Enter/Exit directly.
 **/
class NS_COM_GLUE Monitor : BlockingResourceBase
{
public:
    /**
     * Monitor
     * @param aName A name which can reference this monitor
     * @returns If failure, nsnull
     *          If success, a valid Monitor*, which must be destroyed
     *          by Monitor::DestroyMonitor()
     **/
    Monitor(const char* aName) :
        BlockingResourceBase(aName, eMonitor)
#ifdef DEBUG
        , mEntryCount(0)
#endif
    {
        mMonitor = PR_NewMonitor();
        if (!mMonitor)
            NS_RUNTIMEABORT("Can't allocate mozilla::Monitor");
    }

    /**
     * ~Monitor
     **/
    ~Monitor()
    {
        NS_ASSERTION(mMonitor,
                     "improperly constructed Monitor or double free");
        PR_DestroyMonitor(mMonitor);
        mMonitor = 0;
    }

#ifndef DEBUG
    /** 
     * Enter
     * @see prmon.h 
     **/
    void Enter()
    {
        PR_EnterMonitor(mMonitor);
    }

    /** 
     * Exit
     * @see prmon.h 
     **/
    void Exit()
    {
        PR_ExitMonitor(mMonitor);
    }

    /**
     * Wait
     * @see prmon.h
     **/      
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT)
    {
        return PR_Wait(mMonitor, interval) == PR_SUCCESS ?
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
        return PR_Notify(mMonitor) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

    /** 
     * NotifyAll
     * @see prmon.h 
     **/      
    nsresult NotifyAll()
    {
        return PR_NotifyAll(mMonitor) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    /**
     * AssertCurrentThreadIn
     * @see prmon.h
     **/
    void AssertCurrentThreadIn()
    {
        PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);
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
    Monitor();
    Monitor(const Monitor&);
    Monitor& operator =(const Monitor&);

    PRMonitor* mMonitor;
#ifdef DEBUG
    PRInt32 mEntryCount;
#endif
};


/**
 * MonitorAutoEnter
 * Enters the Monitor when it enters scope, and exits it when it leaves 
 * scope.
 *
 * MUCH PREFERRED to bare calls to Monitor.Enter and Exit.
 */ 
class NS_COM_GLUE NS_STACK_CLASS MonitorAutoEnter
{
public:
    /**
     * Constructor
     * The constructor aquires the given lock.  The destructor
     * releases the lock.
     * 
     * @param aMonitor A valid mozilla::Monitor* returned by 
     *                 mozilla::Monitor::NewMonitor. 
     **/
    MonitorAutoEnter(mozilla::Monitor &aMonitor) :
        mMonitor(&aMonitor)
    {
        NS_ASSERTION(mMonitor, "null monitor");
        mMonitor->Enter();
    }
    
    ~MonitorAutoEnter(void)
    {
        mMonitor->Exit();
    }
 
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT)
    {
       return mMonitor->Wait(interval);
    }

    nsresult Notify()
    {
        return mMonitor->Notify();
    }

    nsresult NotifyAll()
    {
        return mMonitor->Notify();
    }

private:
    MonitorAutoEnter();
    MonitorAutoEnter(const MonitorAutoEnter&);
    MonitorAutoEnter& operator =(const MonitorAutoEnter&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    mozilla::Monitor* mMonitor;
};


} // namespace mozilla


#endif // ifndef mozilla_Monitor_h
