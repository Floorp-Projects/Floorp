/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Brett Wilson <brettw@gmail.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include <stdio.h>

#include "nsError.h"
#include "nsIMutableArray.h"
#include "nsHashSets.h"
#include "nsAutoPtr.h"
#include "nsIFile.h"
#include "nsIVariant.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsThreadUtils.h"

#include "mozIStorageAggregateFunction.h"
#include "mozIStorageFunction.h"

#include "mozStorageEvents.h"
#include "mozStorageUnicodeFunctions.h"
#include "mozStorageConnection.h"
#include "mozStorageService.h"
#include "mozStorageStatement.h"
#include "mozStorageValueArray.h"
#include "mozStoragePrivateHelpers.h"

#include "prlog.h"
#include "prprf.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gStorageLog = nsnull;
#endif

#define PREF_TS_SYNCHRONOUS "toolkit.storage.synchronous"

NS_IMPL_THREADSAFE_ISUPPORTS1(mozStorageConnection, mozIStorageConnection)

mozStorageConnection::mozStorageConnection(mozIStorageService* aService) :
    mDBConn(nsnull)
,   mAsyncExecutionMutex(nsAutoLock::NewLock("AsyncExecutionMutex"))
,   mTransactionMutex(nsAutoLock::NewLock("TransactionMutex"))
,   mTransactionInProgress(PR_FALSE)
,   mFunctionsMutex(nsAutoLock::NewLock("FunctionsMutex"))
,   mProgressHandlerMutex(nsAutoLock::NewLock("ProgressHandlerMutex"))
,   mProgressHandler(nsnull)
,   mStorageService(aService)
{
    mFunctions.Init();
}

mozStorageConnection::~mozStorageConnection()
{
    (void)Close();
    nsAutoLock::DestroyLock(mTransactionMutex);
    nsAutoLock::DestroyLock(mFunctionsMutex);
    nsAutoLock::DestroyLock(mProgressHandlerMutex);
}

#ifdef PR_LOGGING
void tracefunc (void *closure, const char *stmt)
{
    PR_LOG(gStorageLog, PR_LOG_DEBUG, ("sqlite3_trace on %p for '%s'", closure,
                                       stmt));
}
#endif

/**
 * Actually creates the connection from the DB. Called by mozStorageService.
 * You can pass a NULL database file in to get an sqlite in-memory database.
 */
NS_IMETHODIMP
mozStorageConnection::Initialize(nsIFile *aDatabaseFile)
{
    NS_ASSERTION (!mDBConn, "Initialize called on already opened database!");
    NS_ENSURE_TRUE(mTransactionMutex, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_TRUE(mFunctionsMutex, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_TRUE(mProgressHandlerMutex, NS_ERROR_OUT_OF_MEMORY);

    int srv;
    nsresult rv;

    mDatabaseFile = aDatabaseFile;

    if (aDatabaseFile) {
        nsAutoString path;
        rv = aDatabaseFile->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        srv = sqlite3_open (NS_ConvertUTF16toUTF8(path).get(), &mDBConn);
    } else {
        // in memory database requested, sqlite uses a magic file name
        srv = sqlite3_open (":memory:", &mDBConn);
    }
    if (srv != SQLITE_OK) {
        mDBConn = nsnull;
        return ConvertResultCode(srv);
    }

#ifdef PR_LOGGING
    if (! gStorageLog)
        gStorageLog = PR_NewLogModule("mozStorage");

    sqlite3_trace (mDBConn, tracefunc, this);

    nsCAutoString leafName(":memory");
    if (aDatabaseFile)
        (void)aDatabaseFile->GetNativeLeafName(leafName);
    PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Opening connection to '%s' (%p)",
                                        leafName.get(), this));
#endif

    // Hook up i18n functions
    srv = StorageUnicodeFunctions::RegisterFunctions(mDBConn);
    if (srv != SQLITE_OK) {
        mDBConn = nsnull;
        return ConvertResultCode(srv);
    }

    /* Execute a dummy statement to force the db open, and to verify
     * whether it's valid or not
     */
    sqlite3_stmt *stmt = nsnull;
    srv = sqlite3_prepare_v2(mDBConn, "SELECT * FROM sqlite_master", -1, &stmt,
                             NULL);

    if (srv == SQLITE_OK) {
        srv = sqlite3_step(stmt);

        if (srv == SQLITE_DONE || srv == SQLITE_ROW)
            srv = SQLITE_OK;
    } else {
        stmt = nsnull;
    }

    if (stmt != nsnull)
        sqlite3_finalize (stmt);

    if (srv != SQLITE_OK) {
        sqlite3_close (mDBConn);
        mDBConn = nsnull;

        return ConvertResultCode(srv);
    }

    // Set the synchronous PRAGMA, according to the pref
    nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
    PRInt32 synchronous = 1; // Default to NORMAL if pref not set
    if (pref)
        (void)pref->GetIntPref(PREF_TS_SYNCHRONOUS, &synchronous);
    
    switch (synchronous) {
        case 2:
            (void)ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                "PRAGMA synchronous = FULL;"));
            break;
        case 0:
            (void)ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                "PRAGMA synchronous = OFF;"));
            break;
        case 1:
        default:
            (void)ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                "PRAGMA synchronous = NORMAL;"));
            break;
    }

    return NS_OK;
}

/*****************************************************************************
 ** mozIStorageConnection interface
 *****************************************************************************/

/**
 ** Core status/initialization
 **/

NS_IMETHODIMP
mozStorageConnection::Close()
{
    if (!mDBConn)
        return NS_ERROR_NOT_INITIALIZED;

#ifdef PR_LOGGING
    nsCAutoString leafName(":memory");
    if (mDatabaseFile)
        (void)mDatabaseFile->GetNativeLeafName(leafName);
    PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Opening connection to '%s'",
                                        leafName.get()));
#endif

    // The shutdown call runs any pending events to completion, so we want to
    // do this before closing the connection.
    {
        nsAutoLock mutex(mAsyncExecutionMutex);
        if (mAsyncExecutionThread) {
            mAsyncExecutionThread->Shutdown();
            mAsyncExecutionThread = nsnull;
        }
    }

    {
        nsAutoLock mutex(mProgressHandlerMutex);
        if (mProgressHandler)
            sqlite3_progress_handler(mDBConn, 0, NULL, NULL);
    }

    int srv = sqlite3_close(mDBConn);
    if (srv != SQLITE_OK)
        NS_WARNING("sqlite3_close failed. There are probably outstanding statements!");

    mDBConn = NULL;
    return ConvertResultCode(srv);
}

NS_IMETHODIMP
mozStorageConnection::GetConnectionReady(PRBool *aConnectionReady)
{
    *aConnectionReady = (mDBConn != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetDatabaseFile(nsIFile **aFile)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    NS_IF_ADDREF(*aFile = mDatabaseFile);

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetLastInsertRowID(PRInt64 *aLastInsertRowID)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    sqlite_int64 id = sqlite3_last_insert_rowid(mDBConn);
    *aLastInsertRowID = id;

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetLastError(PRInt32 *aLastError)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    *aLastError = sqlite3_errcode(mDBConn);

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetLastErrorString(nsACString& aLastErrorString)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    const char *serr = sqlite3_errmsg(mDBConn);
    aLastErrorString.Assign(serr);

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::GetSchemaVersion(PRInt32 *version)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<mozIStorageStatement> stmt;
    nsresult rv = CreateStatement(NS_LITERAL_CSTRING(
        "PRAGMA user_version"), getter_AddRefs(stmt));
    if (NS_FAILED(rv)) return rv;

    *version = 0;
    PRBool hasResult;
    if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult)
        *version = stmt->AsInt32(0);

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::SetSchemaVersion(PRInt32 aVersion)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    nsCAutoString stmt(NS_LITERAL_CSTRING("PRAGMA user_version = "));
    stmt.AppendInt(aVersion);

    return ExecuteSimpleSQL(stmt);
}

/**
 ** Statements & Queries
 **/

NS_IMETHODIMP
mozStorageConnection::CreateStatement(const nsACString& aSQLStatement,
                                      mozIStorageStatement **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    mozStorageStatement *statement = new mozStorageStatement();
    if (!statement)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(statement);

    nsresult rv = statement->Initialize (this, aSQLStatement);
    if (NS_FAILED(rv)) {
        NS_RELEASE(statement);
        return rv;
    }

    *_retval = statement;
    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::ExecuteSimpleSQL(const nsACString& aSQLStatement)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_exec (mDBConn, PromiseFlatCString(aSQLStatement).get(),
                            NULL, NULL, NULL);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsPromiseFlatCString(aSQLStatement).get());
        return ConvertResultCode(srv);
    }

    return NS_OK;
}

nsresult
mozStorageConnection::ExecuteAsync(mozIStorageStatement ** aStatements,
                                   PRUint32 aNumStatements,
                                   mozIStorageStatementCallback *aCallback,
                                   mozIStoragePendingStatement **_stmt)
{
    int rc = SQLITE_OK;
    nsTArray<sqlite3_stmt *> stmts(aNumStatements);
    for (PRUint32 i = 0; i < aNumStatements && rc == SQLITE_OK; i++) {
        sqlite3_stmt *old_stmt = aStatements[i]->GetNativeStatementPointer();
        NS_ASSERTION(sqlite3_db_handle(old_stmt) == mDBConn,
                     "Statement must be from this database connection!");

        // Clone this statement.  We only need a sqlite3_stmt object, so we can
        // avoid all the extra work that making a new mozStorageStatement would
        // normally involve and use the SQLite API.
        sqlite3_stmt *new_stmt;
        rc = sqlite3_prepare_v2(mDBConn, sqlite3_sql(old_stmt), -1, &new_stmt,
                                NULL);
        if (rc != SQLITE_OK)
            break;

        // Transfer the bindings
        rc = sqlite3_transfer_bindings(old_stmt, new_stmt);
        if (rc != SQLITE_OK)
            break;

        if (!stmts.AppendElement(new_stmt)) {
            rc = SQLITE_NOMEM;
            break;
        }
    }

    // Dispatch to the background
    nsresult rv = NS_OK;
    if (rc == SQLITE_OK)
        rv = NS_executeAsync(stmts, this, aCallback, _stmt);

    // We had a failure, so we need to clean up...
    if (rc != SQLITE_OK || NS_FAILED(rv)) {
        for (PRUint32 i = 0; i < stmts.Length(); i++)
            (void)sqlite3_finalize(stmts[i]);

        if (rc != SQLITE_OK)
            rv = ConvertResultCode(rc);
    }

    // Always reset all the statements
    for (PRUint32 i = 0; i < aNumStatements; i++)
        (void)aStatements[i]->Reset();

    return rv;
}

NS_IMETHODIMP
mozStorageConnection::TableExists(const nsACString& aSQLStatement, PRBool *_retval)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    nsCString query("SELECT name FROM sqlite_master WHERE type = 'table' AND name ='");
    query.Append(aSQLStatement);
    query.AppendLiteral("'");

    sqlite3_stmt *stmt = nsnull;
    int srv = sqlite3_prepare_v2(mDBConn, query.get(), -1, &stmt, NULL);
    if (srv != SQLITE_OK) {
        HandleSqliteError(query.get());
        return ConvertResultCode(srv);
    }

    PRBool exists = PR_FALSE;

    srv = sqlite3_step(stmt);
    // we just care about the return value from step
    sqlite3_finalize(stmt);

    if (srv == SQLITE_ROW) {
        exists = PR_TRUE;
    } else if (srv == SQLITE_DONE) {
        exists = PR_FALSE;
    } else {
        HandleSqliteError("TableExists finalize");
        return ConvertResultCode(srv);
    }

    *_retval = exists;
    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::IndexExists(const nsACString& aIndexName, PRBool* _retval)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    nsCString query("SELECT name FROM sqlite_master WHERE type = 'index' AND name ='");
    query.Append(aIndexName);
    query.AppendLiteral("'");

    sqlite3_stmt *stmt = nsnull;
    int srv = sqlite3_prepare_v2(mDBConn, query.get(), -1, &stmt, NULL);
    if (srv != SQLITE_OK) {
        HandleSqliteError(query.get());
        return ConvertResultCode(srv);
    }

    *_retval = PR_FALSE;

    srv = sqlite3_step(stmt);
    (void)sqlite3_finalize(stmt);

    if (srv == SQLITE_ROW) {
        *_retval = PR_TRUE;
    }

    return ConvertResultCode(srv);
}


/**
 ** Transactions
 **/

NS_IMETHODIMP
mozStorageConnection::GetTransactionInProgress(PRBool *_retval)
{
    nsAutoLock mutex(mTransactionMutex);
    *_retval = mTransactionInProgress;
    return NS_OK;
}

// XXX do we want to just store compiled statements for these?
NS_IMETHODIMP
mozStorageConnection::BeginTransaction()
{
    return BeginTransactionAs(mozIStorageConnection::TRANSACTION_DEFERRED);
}

NS_IMETHODIMP
mozStorageConnection::BeginTransactionAs(PRInt32 aTransactionType)
{
    nsAutoLock mutex(mTransactionMutex);
    if (mTransactionInProgress)
        return NS_ERROR_FAILURE;
    nsresult rv;
    switch(aTransactionType) {
        case TRANSACTION_DEFERRED:
            rv = ExecuteSimpleSQL(NS_LITERAL_CSTRING("BEGIN DEFERRED"));
            break;
        case TRANSACTION_IMMEDIATE:
            rv = ExecuteSimpleSQL(NS_LITERAL_CSTRING("BEGIN IMMEDIATE"));
            break;
        case TRANSACTION_EXCLUSIVE:
            rv = ExecuteSimpleSQL(NS_LITERAL_CSTRING("BEGIN EXCLUSIVE"));
            break;
        default:
            return NS_ERROR_ILLEGAL_VALUE;
    }
    if (NS_SUCCEEDED(rv))
        mTransactionInProgress = PR_TRUE;
    return rv;
}

NS_IMETHODIMP
mozStorageConnection::CommitTransaction()
{
    nsAutoLock mutex(mTransactionMutex);
    if (!mTransactionInProgress)
        return NS_ERROR_FAILURE;
    nsresult rv = ExecuteSimpleSQL(NS_LITERAL_CSTRING("COMMIT TRANSACTION"));
    if (NS_SUCCEEDED(rv))
        mTransactionInProgress = PR_FALSE;
    return rv;
}

NS_IMETHODIMP
mozStorageConnection::RollbackTransaction()
{
    nsAutoLock mutex(mTransactionMutex);
    if (!mTransactionInProgress)
        return NS_ERROR_FAILURE;
    nsresult rv = ExecuteSimpleSQL(NS_LITERAL_CSTRING("ROLLBACK TRANSACTION"));
    if (NS_SUCCEEDED(rv))
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
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;
    int srv;
    char *buf;

    buf = PR_smprintf("CREATE TABLE %s (%s)", aTableName, aTableSchema);
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    srv = sqlite3_exec (mDBConn, buf,
                        NULL, NULL, NULL);

    PR_smprintf_free(buf);

    return ConvertResultCode(srv);
}

/**
 ** Functions
 **/

PLDHashOperator
mozStorageConnection::s_FindFuncEnum(const nsACString &aKey,
                                     nsISupports* aData,
                                     void* userArg)
{
    FindFuncEnumArgs *args = static_cast<FindFuncEnumArgs *>(userArg);
    if ((void*)aData == args->mTarget) {
        args->mFound = PR_TRUE;
        return PL_DHASH_STOP;
    }
    return PL_DHASH_NEXT;
}

PRBool
mozStorageConnection::FindFunctionByInstance(nsISupports *aInstance)
{
    // The lock should already be held by calling functions
    FindFuncEnumArgs args = { aInstance, PR_FALSE };
    mFunctions.EnumerateRead(s_FindFuncEnum, &args);
    return args.mFound;
}

static nsresult
mozStorageVariantToSQLite3Result(sqlite3_context *ctx,
                                 nsIVariant *var)
{
    nsresult rv;
    PRUint16 dt;
    // Allow to return NULL not wrapped to 
    // nsIVariant for speed
    if (!var) {
        sqlite3_result_null (ctx);
        return NS_OK;
    }
    (void)var->GetDataType( &dt );
    switch (dt) {
        case nsIDataType::VTYPE_INT8: 
        case nsIDataType::VTYPE_INT16:
        case nsIDataType::VTYPE_INT32:
        case nsIDataType::VTYPE_UINT8:
        case nsIDataType::VTYPE_UINT16:
            {
                PRInt32 v;
                rv = var->GetAsInt32 (&v);
                if (NS_FAILED(rv)) return rv;
                sqlite3_result_int (ctx, v);
            }
            break;
        case nsIDataType::VTYPE_UINT32: // Try to preserve full range
        case nsIDataType::VTYPE_INT64:
        // Data loss possible, but there is no unsigned types in SQLite
        case nsIDataType::VTYPE_UINT64:
            {
                PRInt64 v;
                rv = var->GetAsInt64 (&v);
                if (NS_FAILED(rv)) return rv;
                sqlite3_result_int64 (ctx, v);
            }
            break;
        case nsIDataType::VTYPE_FLOAT:
        case nsIDataType::VTYPE_DOUBLE:
            {
                double v;
                rv = var->GetAsDouble (&v);
                if (NS_FAILED(rv)) return rv;
                sqlite3_result_double (ctx, v);
            }
            break;
        case nsIDataType::VTYPE_BOOL:
            {
                PRBool v;
                rv = var->GetAsBool(&v);
                if (NS_FAILED(rv)) return rv;
                sqlite3_result_int (ctx, v ? 1 : 0);
            }
            break;
        case nsIDataType::VTYPE_CHAR:
        case nsIDataType::VTYPE_WCHAR:
        case nsIDataType::VTYPE_DOMSTRING:
        case nsIDataType::VTYPE_CHAR_STR:
        case nsIDataType::VTYPE_WCHAR_STR:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        case nsIDataType::VTYPE_UTF8STRING:
        case nsIDataType::VTYPE_CSTRING:
        case nsIDataType::VTYPE_ASTRING:
            {
                nsAutoString v;
                // GetAsAString does proper conversion to UCS2
                // from all string-like types. It can be used
                // universally without problems.
                rv = var->GetAsAString (v);
                if (NS_FAILED(rv)) return rv;
                sqlite3_result_text16 (ctx, 
                                       nsPromiseFlatString(v).get(),
                                       v.Length() * 2,
                                       SQLITE_TRANSIENT);
            }
            break;
        case nsIDataType::VTYPE_VOID:
        case nsIDataType::VTYPE_EMPTY:
            sqlite3_result_null (ctx);
            break;
        // Maybe, it'll be possible to convert these
        // in future too.
        case nsIDataType::VTYPE_ID:
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
        case nsIDataType::VTYPE_ARRAY:
        case nsIDataType::VTYPE_EMPTY_ARRAY:
        default:
            return NS_ERROR_CANNOT_CONVERT_DATA;
    }
    return NS_OK;
}

static void
mozStorageSqlFuncHelper (sqlite3_context *ctx,
                         int argc,
                         sqlite3_value **argv)
{
    void *userData = sqlite3_user_data (ctx);
    // We don't want to QI here, because this will be called a -lot-
    mozIStorageFunction *userFunction =
        static_cast<mozIStorageFunction *>(userData);

    nsRefPtr<mozStorageArgvValueArray> ava = new mozStorageArgvValueArray (argc, argv);
    if (!ava)
        return;
    nsCOMPtr<nsIVariant> retval;
    nsresult rv = userFunction->OnFunctionCall (ava, getter_AddRefs(retval));
    if (NS_FAILED(rv)) {
        NS_WARNING("mozIStorageConnection: User function returned error code!\n");
        sqlite3_result_error(ctx,
                             "User function returned error code",
                             -1);
        return;
    }
    rv = mozStorageVariantToSQLite3Result(ctx,retval);
    if (NS_FAILED(rv)) {
        NS_WARNING("mozIStorageConnection: User function returned invalid data type!\n");
        sqlite3_result_error(ctx,
                             "User function returned invalid data type",
                             -1);
    }
}

NS_IMETHODIMP
mozStorageConnection::CreateFunction(const nsACString &aFunctionName,
                                     PRInt32 aNumArguments,
                                     mozIStorageFunction *aFunction)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    // do we already have this function defined?
    // Check for name only because simple function can
    // be defined multiple times with different names (aliases).
    nsAutoLock mutex(mFunctionsMutex);
    NS_ENSURE_FALSE(mFunctions.Get (aFunctionName, NULL), NS_ERROR_FAILURE);

    int srv = sqlite3_create_function (mDBConn,
                                       nsPromiseFlatCString(aFunctionName).get(),
                                       aNumArguments,
                                       SQLITE_ANY,
                                       aFunction,
                                       mozStorageSqlFuncHelper,
                                       NULL,
                                       NULL);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsnull);
        return ConvertResultCode(srv);
    }

    if (mFunctions.Put (aFunctionName, aFunction)) {
        return NS_OK;
    }
    return NS_ERROR_OUT_OF_MEMORY;
}

static void
mozStorageSqlAggrFuncStepHelper (sqlite3_context *ctx,
                                 int argc,
                                 sqlite3_value **argv)
{
    void *userData = sqlite3_user_data (ctx);
    // We don't want to QI here, because this will be called a -lot-
    mozIStorageAggregateFunction *userFunction =
        static_cast<mozIStorageAggregateFunction *>(userData);

    nsRefPtr<mozStorageArgvValueArray> ava =
        new mozStorageArgvValueArray (argc, argv);
    if (!ava)
        return;
    nsresult rv = userFunction->OnStep(ava);
    if (NS_FAILED(rv))
        NS_WARNING("mozIStorageConnection: User aggregate step function returned error code!\n");
}

static void
mozStorageSqlAggrFuncFinalHelper (sqlite3_context *ctx)
{
    void *userData = sqlite3_user_data (ctx);
    // We don't want to QI here, because this will be called a -lot-
    mozIStorageAggregateFunction *userFunction =
        static_cast<mozIStorageAggregateFunction *>(userData);

    nsRefPtr<nsIVariant> retval;
    nsresult rv = userFunction->OnFinal (getter_AddRefs(retval));
    if (NS_FAILED(rv)) {
        NS_WARNING("mozIStorageConnection: User aggregate final function returned error code!\n");
        sqlite3_result_error(ctx,
                             "User aggregate final function returned error code",
                             -1);
        return;
    }
    rv = mozStorageVariantToSQLite3Result(ctx,retval);
    if (NS_FAILED(rv)) {
        NS_WARNING("mozIStorageConnection: User aggregate final function returned invalid data type!\n");
        sqlite3_result_error(ctx,
                             "User aggregate final function returned invalid data type",
                             -1);
    }
}

NS_IMETHODIMP
mozStorageConnection::CreateAggregateFunction(const nsACString &aFunctionName,
                                              PRInt32 aNumArguments,
                                              mozIStorageAggregateFunction *aFunction)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    // do we already have this function defined?
    // Check for name.
    nsAutoLock mutex(mFunctionsMutex);
    NS_ENSURE_FALSE(mFunctions.Get (aFunctionName, NULL), NS_ERROR_FAILURE);

    // Aggregate functions are stateful, so we cannot have
    // aliases for them.
    // Enumerate all functions and determine if this one is already
    // implemented
    NS_ENSURE_FALSE(FindFunctionByInstance (aFunction), NS_ERROR_FAILURE);

    int srv = sqlite3_create_function (mDBConn,
                                       nsPromiseFlatCString(aFunctionName).get(),
                                       aNumArguments,
                                       SQLITE_ANY,
                                       aFunction,
                                       NULL,
                                       mozStorageSqlAggrFuncStepHelper,
                                       mozStorageSqlAggrFuncFinalHelper);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsnull);
        return ConvertResultCode(srv);
    }

    if (mFunctions.Put (aFunctionName, aFunction)) {
        return NS_OK;
    }
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
mozStorageConnection::RemoveFunction(const nsACString &aFunctionName)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    nsAutoLock mutex(mFunctionsMutex);
    NS_ENSURE_TRUE(mFunctions.Get (aFunctionName, NULL), NS_ERROR_FAILURE);

    int srv = sqlite3_create_function (mDBConn,
                                       nsPromiseFlatCString(aFunctionName).get(),
                                       0,
                                       SQLITE_ANY,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);
    if (srv != SQLITE_OK) {
        HandleSqliteError(nsnull);
        return ConvertResultCode(srv);
    }

    mFunctions.Remove (aFunctionName);

    return NS_OK;
}

int
mozStorageConnection::s_ProgressHelper(void *arg)
{
  mozStorageConnection *_this = static_cast<mozStorageConnection *>(arg);
  return _this->ProgressHandler();
}

NS_IMETHODIMP
mozStorageConnection::SetProgressHandler(PRInt32 aGranularity,
                                         mozIStorageProgressHandler *aHandler,
                                         mozIStorageProgressHandler **aOldHandler)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    // Return previous one
    nsAutoLock mutex(mProgressHandlerMutex);
    NS_IF_ADDREF(*aOldHandler = mProgressHandler);

    if (!aHandler || aGranularity <= 0) {
      aHandler     = nsnull;
      aGranularity = 0;
    }
    mProgressHandler = aHandler;
    sqlite3_progress_handler (mDBConn, aGranularity, s_ProgressHelper, this);

    return NS_OK;
}

NS_IMETHODIMP
mozStorageConnection::RemoveProgressHandler(mozIStorageProgressHandler **aOldHandler)
{
    if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

    // Return previous one
    nsAutoLock mutex(mProgressHandlerMutex);
    NS_IF_ADDREF(*aOldHandler = mProgressHandler);

    mProgressHandler = nsnull;
    sqlite3_progress_handler (mDBConn, 0, NULL, NULL);

    return NS_OK;
}

int
mozStorageConnection::ProgressHandler()
{
    nsAutoLock mutex(mProgressHandlerMutex);
    if (mProgressHandler) {
        PRBool res;
        nsresult rv = mProgressHandler->OnProgress(this, &res);
        if (NS_FAILED(rv)) return 0; // Don't break request
        return res ? 1 : 0;
    }
    return 0;
}

/**
 ** Other bits
 **/

already_AddRefed<nsIEventTarget>
mozStorageConnection::getAsyncExecutionTarget()
{
    nsAutoLock mutex(mAsyncExecutionMutex);

    if (!mAsyncExecutionThread) {
        nsresult rv = NS_NewThread(getter_AddRefs(mAsyncExecutionThread));
        if (NS_FAILED(rv)) {
            NS_WARNING("Failed to create async thread.");
            return nsnull;
        }
    }

    nsIEventTarget *eventTarget;
    NS_ADDREF(eventTarget = mAsyncExecutionThread);
    return eventTarget;
}

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
