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

#include <stdio.h>

#include "nsError.h"
#include "nsArray.h"
#include "nsIFile.h"

#include "mozIStorageFunction.h"

#include "mozStorageConnection.h"
#include "mozStorageStatement.h"
#include "mozStorageValueArray.h"

#include "prlog.h"
#include "prprf.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gStorageLog = nsnull;
#endif

NS_IMPL_ISUPPORTS1(mozStorageConnection, mozIStorageConnection)

mozStorageConnection::mozStorageConnection()
    : mDBConn(nsnull), mTransactionInProgress(PR_FALSE)
{
    
}

mozStorageConnection::~mozStorageConnection()
{
    if (mDBConn) {
        int srv = sqlite3_close (mDBConn);
        if (srv != SQLITE_OK) {
            NS_WARNING("sqlite3_close failed.  There are probably outstanding statements!");
        }
    }
}

#ifdef PR_LOGGING
void tracefunc (void *closure, const char *stmt)
{
    PR_LOG(gStorageLog, PR_LOG_DEBUG, ("%s", stmt));
}
#endif

NS_IMETHODIMP
mozStorageConnection::Initialize(nsIFile *aDatabaseFile)
{
    NS_ENSURE_ARG_POINTER(aDatabaseFile);

    NS_ASSERTION (!mDBConn, "Initialize called on already opened database!");

    int srv;
    nsresult rv;

    rv = aDatabaseFile->GetNativeLeafName(mDatabaseName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString nativePath;
    rv = aDatabaseFile->GetNativePath(nativePath);
    NS_ENSURE_SUCCESS(rv, rv);

    srv = sqlite3_open (nativePath.get(), &mDBConn);
    if (srv != SQLITE_OK) {
        mDBConn = nsnull;
        return NS_ERROR_FAILURE; // XXX error code
    }

#ifdef PR_LOGGING
    if (! gStorageLog)
        gStorageLog = PR_NewLogModule("mozStorage");

    sqlite3_trace (mDBConn, tracefunc, nsnull);
#endif

    rv = NS_NewArray(getter_AddRefs(mFunctions));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

/*****************************************************************************
 ** mozIStorageConnection interface
 *****************************************************************************/

/**
 ** Core status/initialization
 **/

NS_IMETHODIMP
mozStorageConnection::GetConnectionReady(PRBool *aConnectionReady)
{
    *aConnectionReady = (mDBConn != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetDatabaseName(nsACString& aDatabaseName)
{
    NS_ASSERTION(mDBConn, "connection not initialized");

    aDatabaseName = mDatabaseName;

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetLastInsertRowID(PRInt64 *aLastInsertRowID)
{
    NS_ASSERTION(mDBConn, "connection not initialized");

    sqlite_int64 id = sqlite3_last_insert_rowid(mDBConn);
    *aLastInsertRowID = id;

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetLastError(PRInt32 *aLastError)
{
    NS_ASSERTION(mDBConn, "connection not initialized");

    *aLastError = sqlite3_errcode(mDBConn);

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetLastErrorString(nsACString& aLastErrorString)
{
    NS_ASSERTION(mDBConn, "connection not initialized");

    const char *serr = sqlite3_errmsg(mDBConn);
    aLastErrorString.Assign(serr);

    return NS_OK;
}

/**
 ** Statements & Queries
 **/

NS_IMETHODIMP
mozStorageConnection::CreateStatement(const nsACString& aSQLStatement,
                                      mozIStorageStatement **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    NS_ASSERTION(mDBConn, "connection not initialized");

    mozStorageStatement *statement = new mozStorageStatement();
    NS_ADDREF(statement);

    nsresult rv = statement->Initialize (this, aSQLStatement);
    if (NS_FAILED(rv)) {
        NS_RELEASE(statement);
        return NS_ERROR_FAILURE; // XXX error code
    }

    *_retval = statement;
    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::ExecuteSimpleSQL(const nsACString& aSQLStatement)
{
    NS_ENSURE_ARG_POINTER(mDBConn);

    char *errMsg = NULL;
    int srv = sqlite3_exec (mDBConn, PromiseFlatCString(aSQLStatement).get(),
                            NULL, NULL, NULL);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsPromiseFlatCString(aSQLStatement).get());
        return NS_ERROR_FAILURE; // XXX error code
    }

    return NS_OK;
}

/**
 ** Transactions
 **/

NS_IMETHODIMP
mozStorageConnection::GetTransactionInProgress(PRInt32 *_retval)
{
    *_retval = mTransactionInProgress;
    return NS_OK;
}

// XXX do we want to just store compiled statements for these?
NS_IMETHODIMP
mozStorageConnection::BeginTransaction()
{
    if (mTransactionInProgress)
        return NS_ERROR_FAILURE; // XXX error code
    nsresult rv = ExecuteSimpleSQL (NS_LITERAL_CSTRING("BEGIN TRANSACTION"));
    if (NS_SUCCEEDED(rv))
        mTransactionInProgress = PR_TRUE;
    return rv;
}

NS_IMETHODIMP
mozStorageConnection::CommitTransaction()
{
    if (!mTransactionInProgress)
        return NS_ERROR_FAILURE;
    nsresult rv = ExecuteSimpleSQL (NS_LITERAL_CSTRING("COMMIT TRANSACTION"));
    // even if the commit fails, the transaction is aborted
    mTransactionInProgress = PR_FALSE;
    return rv;
}

NS_IMETHODIMP
mozStorageConnection::RollbackTransaction()
{
    if (!mTransactionInProgress)
        return NS_ERROR_FAILURE;
    nsresult rv = ExecuteSimpleSQL (NS_LITERAL_CSTRING("ROLLBACK TRANSACTION"));
    mTransactionInProgress = PR_FALSE;
    return rv;
}

/**
 ** Table creation
 **/

NS_IMETHODIMP
mozStorageConnection::CreateTable(/*const nsID& aID,*/
                                  const char *aTableName,
                                  const char *aTableSchema)
{
    int srv;
    char *buf;
    int buflen = 0;

#if 0
    buflen = snprintf(nsnull, 0, "CREATE TABLE %s (%s)");
    if (buflen <= 0)
        return NS_ERROR_FAILURE;

    buf = nsMemory::Alloc(buflen + 1);
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    buflen = sprintf(buf, buflen, "CREATE TABLE %s (%s)");
    if (buflen <= 0) {
        nsMemory::Free(buf);
        return NS_ERROR_FAILURE;
    }

    srv = sqlite3_exec (mDBConn, buf,
                        NULL, NULL, NULL);
    nsMemory::Free(buf);

    if (srv != SQLITE_OK) {
        return NS_ERROR_FAILURE; // XXX SQL_ERROR_TABLE_EXISTS
    }
#endif
    return NS_OK;
}

/**
 ** Functions and Triggers
 **/

static void
mozStorageSqlFuncHelper (sqlite3_context *ctx,
                         int argc,
                         sqlite3_value **argv)
{
    fprintf (stderr, "mozStorageSqlFuncHelper: %p %d %p\n", ctx, argc, argv);

    void *userData = sqlite3_user_data (ctx);
    // We don't want to QI here, because this will be called a -lot-
    mozIStorageFunction *userFunction = NS_STATIC_CAST(mozIStorageFunction *, userData);

    nsCOMPtr<mozStorageArgvValueArray> ava = new mozStorageArgvValueArray (argc, argv);
    nsresult rv = userFunction->OnFunctionCall (ava);
    if (NS_FAILED(rv)) {
        NS_WARNING("mozIStorageConnection: User function returned error code!\n");
    }
}

NS_IMETHODIMP
mozStorageConnection::CreateFunction(const char *aFunctionName,
                                     PRInt32 aNumArguments,
                                     mozIStorageFunction *aFunction)
{
    nsresult rv;

    // do we already have this function defined?
    // XXX check for name as well
    PRUint32 idx;
    rv = mFunctions->IndexOf (0, aFunction, &idx);
    if (rv != NS_ERROR_FAILURE) {
        // already exists
        return NS_ERROR_FAILURE;
    }

    int srv = sqlite3_create_function (mDBConn,
                                       aFunctionName,
                                       aNumArguments,
                                       SQLITE_ANY,
                                       aFunction,
                                       mozStorageSqlFuncHelper,
                                       nsnull,
                                       nsnull);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsnull);
        return NS_ERROR_FAILURE;
    }

    rv = mFunctions->AppendElement (aFunction, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::CreateTrigger(const char *aTriggerName,
                                    PRInt32 aTriggerType,
                                    const char *aTableName,
                                    const char *aTriggerFunction,
                                    const char *aParameters)
{
    nsresult rv;

#if 0
    /* We don't need to split this until we need to generate
     * our own IPC trigger
     */
    nsCStringArray *cstr = new nsCStringArray();
    if (!cstr)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = cstr->ParseString (aParameters, ",");
    if (NS_FAILED(rv)) return rv;
#endif

    char *event = nsnull;
    if (aTriggerType == TRIGGER_EVENT_DELETE)
        event = "DELETE";
    else if (aTriggerType == TRIGGER_EVENT_INSERT)
        event = "INSERT";
    else if (aTriggerType == TRIGGER_EVENT_UPDATE)
        event = "UPDATE";
    else
        return NS_ERROR_FAILURE;

    char *sql = PR_sprintf_append
        (nsnull,
         "CREATE TEMPORARY TRIGGER %s AFTER %s ON %s FOR EACH ROW BEGIN SELECT %s(%s); END;",
         aTriggerName,
         event,
         aTableName,
         aTriggerFunction,
         aParameters);
    if (!sql)
        return NS_ERROR_OUT_OF_MEMORY;

    /* Now create the trigger */
    int srv = sqlite3_exec (mDBConn,
                            sql,
                            nsnull,
                            nsnull,
                            nsnull);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsnull);
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::RemoveTrigger(const char *aTriggerName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 ** Native support
 **/
NS_IMETHODIMP
mozStorageConnection::GetSqliteHandle(sqlite3 **aSqliteHandle)
{
    *aSqliteHandle = mDBConn;
    return NS_OK;
}

/**
 ** Other bits
 **/
void
mozStorageConnection::HandleSqliteError(const char *aSqlStatement)
{
    // an error just occured!
#ifdef PR_LOGGING
    PR_LOG(gStorageLog, PR_LOG_DEBUG, ("Sqlite error: %d '%s'", sqlite3_errcode(mDBConn), sqlite3_errmsg(mDBConn)));
    if (aSqlStatement)
        PR_LOG(gStorageLog, PR_LOG_DEBUG, ("Statement was: %s", aSqlStatement));
#endif
}
