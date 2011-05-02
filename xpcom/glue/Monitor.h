/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyrigght (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#ifndef mozilla_Monitor_h
#define mozilla_Monitor_h

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

namespace mozilla {

/**
 * Monitor provides a *non*-reentrant monitor: *not* a Java-style
 * monitor.  If your code needs support for reentrancy, use
 * ReentrantMonitor instead.  (Rarely should reentrancy be needed.)
 *
 * Instead of directly calling Monitor methods, it's safer and simpler
 * to instead use the RAII wrappers MonitorAutoLock and
 * MonitorAutoUnlock.
 */
class NS_COM_GLUE Monitor
{
public:
    Monitor(const char* aName) :
        mMutex(aName),
        mCondVar(mMutex, "[Monitor.mCondVar]")
    {}

    ~Monitor() {}

    void Lock()
    {
        mMutex.Lock();
    }

    void Unlock()
    {
        mMutex.Unlock();
    }

    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT)
    {
        return mCondVar.Wait(interval);
    }

    nsresult Notify()
    {
        return mCondVar.Notify();
    }

    nsresult NotifyAll()
    {
        return mCondVar.NotifyAll();
    }

    void AssertCurrentThreadOwns() const
    {
        mMutex.AssertCurrentThreadOwns();
    }

    void AssertNotCurrentThreadOwns() const
    {
        mMutex.AssertNotCurrentThreadOwns();
    }

private:
    Monitor();
    Monitor(const Monitor&);
    Monitor& operator =(const Monitor&);

    Mutex mMutex;
    CondVar mCondVar;
};

/**
 * Lock the monitor for the lexical scope instances of this class are
 * bound to (except for MonitorAutoUnlock in nested scopes).
 *
 * The monitor must be unlocked when instances of this class are
 * created.
 */
class NS_COM_GLUE NS_STACK_CLASS MonitorAutoLock
{
public:
    MonitorAutoLock(Monitor& aMonitor) :
        mMonitor(&aMonitor)
    {
        mMonitor->Lock();
    }
    
    ~MonitorAutoLock()
    {
        mMonitor->Unlock();
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
        return mMonitor->NotifyAll();
    }

private:
    MonitorAutoLock();
    MonitorAutoLock(const MonitorAutoLock&);
    MonitorAutoLock& operator =(const MonitorAutoLock&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    Monitor* mMonitor;
};

/**
 * Unlock the monitor for the lexical scope instances of this class
 * are bound to (except for MonitorAutoLock in nested scopes).
 *
 * The monitor must be locked by the current thread when instances of
 * this class are created.
 */
class NS_COM_GLUE NS_STACK_CLASS MonitorAutoUnlock
{
public:
    MonitorAutoUnlock(Monitor& aMonitor) :
        mMonitor(&aMonitor)
    {
        mMonitor->Unlock();
    }
    
    ~MonitorAutoUnlock()
    {
        mMonitor->Lock();
    }
 
private:
    MonitorAutoUnlock();
    MonitorAutoUnlock(const MonitorAutoUnlock&);
    MonitorAutoUnlock& operator =(const MonitorAutoUnlock&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    Monitor* mMonitor;
};

} // namespace mozilla

#endif // mozilla_Monitor_h
