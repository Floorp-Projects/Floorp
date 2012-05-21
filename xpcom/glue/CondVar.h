/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CondVar_h
#define mozilla_CondVar_h

#include "prcvar.h"

#include "mozilla/BlockingResourceBase.h"
#include "mozilla/Mutex.h"

namespace mozilla {


/**
 * CondVar
 * Vanilla condition variable.  Please don't use this unless you have a 
 * compelling reason --- Monitor provides a simpler API.
 */
class NS_COM_GLUE CondVar : BlockingResourceBase
{
public:
    /**
     * CondVar
     *
     * The CALLER owns |lock|.
     *
     * @param aLock A Mutex to associate with this condition variable.
     * @param aName A name which can reference this monitor
     * @returns If failure, nsnull.
     *          If success, a valid Monitor* which must be destroyed
     *          by Monitor::DestroyMonitor()
     **/
    CondVar(Mutex& aLock, const char* aName) :
        BlockingResourceBase(aName, eCondVar),
        mLock(&aLock)
    {
        MOZ_COUNT_CTOR(CondVar);
        // |lock| must necessarily already be known to the deadlock detector
        mCvar = PR_NewCondVar(mLock->mLock);
        if (!mCvar)
            NS_RUNTIMEABORT("Can't allocate mozilla::CondVar");
    }

    /**
     * ~CondVar
     * Clean up after this CondVar, but NOT its associated Mutex.
     **/
    ~CondVar()
    {
        NS_ASSERTION(mCvar && mLock,
                     "improperly constructed CondVar or double free");
        PR_DestroyCondVar(mCvar);
        mCvar = 0;
        mLock = 0;
        MOZ_COUNT_DTOR(CondVar);
    }

#ifndef DEBUG
    /** 
     * Wait
     * @see prcvar.h 
     **/      
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT)
    {
        // NSPR checks for lock ownership
        return PR_WaitCondVar(mCvar, interval) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }
#else
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT);
#endif // ifndef DEBUG

    /** 
     * Notify
     * @see prcvar.h 
     **/      
    nsresult Notify()
    {
        // NSPR checks for lock ownership
        return PR_NotifyCondVar(mCvar) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

    /** 
     * NotifyAll
     * @see prcvar.h 
     **/      
    nsresult NotifyAll()
    {
        // NSPR checks for lock ownership
        return PR_NotifyAllCondVar(mCvar) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    /**
     * AssertCurrentThreadOwnsMutex
     * @see Mutex::AssertCurrentThreadOwns
     **/
    void AssertCurrentThreadOwnsMutex()
    {
        mLock->AssertCurrentThreadOwns();
    }

    /**
     * AssertNotCurrentThreadOwnsMutex
     * @see Mutex::AssertNotCurrentThreadOwns
     **/
    void AssertNotCurrentThreadOwnsMutex()
    {
        mLock->AssertNotCurrentThreadOwns();
    }

#else
    void AssertCurrentThreadOwnsMutex()
    {
    }
    void AssertNotCurrentThreadOwnsMutex()
    {
    }

#endif  // ifdef DEBUG

private:
    CondVar();
    CondVar(CondVar&);
    CondVar& operator=(CondVar&);

    Mutex* mLock;
    PRCondVar* mCvar;
};


} // namespace mozilla


#endif  // ifndef mozilla_CondVar_h  
