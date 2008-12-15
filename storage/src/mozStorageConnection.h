/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Lev Serebryakov <lev@serebryakov.spb.ru>
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

#ifndef _MOZSTORAGECONNECTION_H_
#define _MOZSTORAGECONNECTION_H_

#include "nsCOMPtr.h"
#include "nsAutoLock.h"

#include "nsString.h"
#include "nsInterfaceHashtable.h"
#include "mozIStorageProgressHandler.h"
#include "mozIStorageConnection.h"

#include "nsIMutableArray.h"

#include <sqlite3.h>

class nsIFile;
class nsIEventTarget;
class nsIThread;
class mozIStorageService;

class mozStorageConnection : public mozIStorageConnection
{
public:

    mozStorageConnection(mozIStorageService* aService);

    NS_IMETHOD Initialize(nsIFile *aDatabaseFile);

    // interfaces
    NS_DECL_ISUPPORTS
    NS_DECL_MOZISTORAGECONNECTION

    // fetch the native handle
    sqlite3 *GetNativeConnection() { return mDBConn; }

    /**
     * Lazily creates and returns a background execution thread.  In the future,
     * the thread may be re-claimed if left idle, so you should call this
     * method just before you dispatch and not save the reference.
     * 
     * @returns an event target suitable for asynchronous statement execution.
     */
    already_AddRefed<nsIEventTarget> getAsyncExecutionTarget();

private:
    ~mozStorageConnection();

protected:
    struct FindFuncEnumArgs {
        nsISupports *mTarget;
        PRBool       mFound;
    };

    void HandleSqliteError(const char *aSqlStatement);
    static PLDHashOperator s_FindFuncEnum(const nsACString &aKey,
                                          nsISupports* aData, void* userArg);
    PRBool FindFunctionByInstance(nsISupports *aInstance);

    static int s_ProgressHelper(void *arg);
    // Generic progress handler
    // Dispatch call to registered progress handler,
    // if there is one. Do nothing in other cases.
    int ProgressHandler();

    sqlite3 *mDBConn;
    nsCOMPtr<nsIFile> mDatabaseFile;

    /**
     * Protects access to mAsyncExecutionThread.
     */
    PRLock *mAsyncExecutionMutex;

    /**
     * Lazily created thread for asynchronous statement execution.  Consumers
     * should use getAsyncExecutionTarget rather than directly accessing this
     * field.
     */
    nsCOMPtr<nsIThread> mAsyncExecutionThread;
    /**
     * Set to true by Close() prior to actually shutting down the thread.  This
     * lets getAsyncExecutionTarget() know not to hand out any more thread
     * references (or to create the thread in the first place).  This variable
     * should be accessed while holding the mAsyncExecutionMutex.
     */
    PRBool mAsyncExecutionThreadShuttingDown;

    PRLock *mTransactionMutex;
    PRBool mTransactionInProgress;

    PRLock *mFunctionsMutex;
    nsInterfaceHashtable<nsCStringHashKey, nsISupports> mFunctions;

    PRLock *mProgressHandlerMutex;
    nsCOMPtr<mozIStorageProgressHandler> mProgressHandler;

    // This isn't accessed but is used to make sure that the connections do
    // not outlive the service. The service, for example, owns certain locks
    // in mozStorageAsyncIO file that the connections depend on.
    nsCOMPtr<mozIStorageService> mStorageService;
};

#endif /* _MOZSTORAGECONNECTION_H_ */
