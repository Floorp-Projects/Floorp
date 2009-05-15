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

#ifndef mozilla_CondVar_h
#define mozilla_CondVar_h

#include "prcvar.h"

#include "mozilla/BlockingResourceBase.h"
#include "mozilla/Mutex.h"

namespace mozilla {


/**
 * CondVar
 * Vanilla condition variable.  Please don't use this unless you have a 
 * compelling reason --- Monitor provides a better API.
 **/
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
