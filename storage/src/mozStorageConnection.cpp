/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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

#include "mozStorageAsyncStatementExecution.h"
#include "mozStorageSQLFunctions.h"
#include "mozStorageConnection.h"
#include "mozStorageService.h"
#include "mozStorageStatement.h"
#include "mozStorageArgValueArray.h"
#include "mozStoragePrivateHelpers.h"

#include "prlog.h"
#include "prprf.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gStorageLog = nsnull;
#endif

namespace mozilla {
namespace storage {

#define PREF_TS_SYNCHRONOUS "toolkit.storage.synchronous"

////////////////////////////////////////////////////////////////////////////////
//// Local Functions

namespace {
#ifdef PR_LOGGING
void tracefunc (void *aClosure, const char *aStmt)
{
  PR_LOG(gStorageLog, PR_LOG_DEBUG, ("sqlite3_trace on %p for '%s'", aClosure,
                                     aStmt));
}
#endif

struct FFEArguments
{
    nsISupports *target;
    bool found;
};
PLDHashOperator
findFunctionEnumerator(const nsACString &aKey,
                       nsISupports *aData,
                       void *aUserArg)
{
  FFEArguments *args = static_cast<FFEArguments *>(aUserArg);
  if (aData == args->target) {
    args->found = PR_TRUE;
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

nsresult
VariantToSQLite3Result(sqlite3_context *aCtx,
                       nsIVariant *aValue)
{
  // Allow to return NULL not wrapped to nsIVariant for speed.
  if (!aValue) {
    ::sqlite3_result_null(aCtx);
    return NS_OK;
  }

  PRUint16 type;
  (void)aValue->GetDataType(&type);
  switch (type) {
    case nsIDataType::VTYPE_INT8: 
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    {
      PRInt32 value;
      nsresult rv = aValue->GetAsInt32(&value);
      NS_ENSURE_SUCCESS(rv, rv);
      ::sqlite3_result_int(aCtx, value);
      break;
    }
    case nsIDataType::VTYPE_UINT32: // Try to preserve full range
    case nsIDataType::VTYPE_INT64:
    // Data loss possible, but there is no unsigned types in SQLite
    case nsIDataType::VTYPE_UINT64:
    {
      PRInt64 value;
      nsresult rv = aValue->GetAsInt64(&value);
      NS_ENSURE_SUCCESS(rv, rv);
      ::sqlite3_result_int64(aCtx, value);
      break;
    }
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    {
      double value;
      nsresult rv = aValue->GetAsDouble(&value);
      NS_ENSURE_SUCCESS(rv, rv);
      ::sqlite3_result_double(aCtx, value);
      break;
    }
    case nsIDataType::VTYPE_BOOL:
    {
      PRBool value;
      nsresult rv = aValue->GetAsBool(&value);
      NS_ENSURE_SUCCESS(rv, rv);
      ::sqlite3_result_int(aCtx, value ? 1 : 0);
      break;
    }
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
      nsAutoString value;
      // GetAsAString does proper conversion to UCS2 from all string-like types.
      // It can be used universally without problems.
      nsresult rv = aValue->GetAsAString(value);
      NS_ENSURE_SUCCESS(rv, rv);
      ::sqlite3_result_text16(aCtx,
                              nsPromiseFlatString(value).get(),
                              value.Length() * 2, // Number of bytes!
                              SQLITE_TRANSIENT);
      break;
    }
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      ::sqlite3_result_null(aCtx);
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

void
basicFunctionHelper(sqlite3_context *aCtx,
                    int aArgc,
                    sqlite3_value **aArgv)
{
  void *userData = ::sqlite3_user_data(aCtx);

  mozIStorageFunction *func = static_cast<mozIStorageFunction *>(userData);

  nsRefPtr<ArgValueArray> arguments(new ArgValueArray(aArgc, aArgv));
  if (!arguments)
      return;

  nsCOMPtr<nsIVariant> result;
  if (NS_FAILED(func->OnFunctionCall(arguments, getter_AddRefs(result)))) {
    NS_WARNING("User function returned error code!");
    ::sqlite3_result_error(aCtx,
                           "User function returned error code",
                           -1);
    return;
  }
  if (NS_FAILED(VariantToSQLite3Result(aCtx, result))) {
    NS_WARNING("User function returned invalid data type!");
    ::sqlite3_result_error(aCtx,
                           "User function returned invalid data type",
                           -1);
  }
}

void
aggregateFunctionStepHelper(sqlite3_context *aCtx,
                            int aArgc,
                            sqlite3_value **aArgv)
{
  void *userData = ::sqlite3_user_data(aCtx);
  mozIStorageAggregateFunction *func =
    static_cast<mozIStorageAggregateFunction *>(userData);

  nsRefPtr<ArgValueArray> arguments(new ArgValueArray(aArgc, aArgv));
  if (!arguments)
    return;

  if (NS_FAILED(func->OnStep(arguments)))
    NS_WARNING("User aggregate step function returned error code!");
}

void
aggregateFunctionFinalHelper(sqlite3_context *aCtx)
{
  void *userData = ::sqlite3_user_data(aCtx);
  mozIStorageAggregateFunction *func =
    static_cast<mozIStorageAggregateFunction *>(userData);

  nsRefPtr<nsIVariant> result;
  if (NS_FAILED(func->OnFinal(getter_AddRefs(result)))) {
    NS_WARNING("User aggregate final function returned error code!");
    ::sqlite3_result_error(aCtx,
                           "User aggregate final function returned error code",
                           -1);
    return;
  }

  if (NS_FAILED(VariantToSQLite3Result(aCtx, result))) {
    NS_WARNING("User aggregate final function returned invalid data type!");
    ::sqlite3_result_error(aCtx,
                           "User aggregate final function returned invalid data type",
                           -1);
  }
}


} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// Connection

Connection::Connection(mozIStorageService *aService)
: mDBConn(nsnull)
, mAsyncExecutionMutex(nsAutoLock::NewLock("AsyncExecutionMutex"))
, mAsyncExecutionThreadShuttingDown(PR_FALSE)
, mTransactionMutex(nsAutoLock::NewLock("TransactionMutex"))
, mTransactionInProgress(PR_FALSE)
, mFunctionsMutex(nsAutoLock::NewLock("FunctionsMutex"))
, mProgressHandlerMutex(nsAutoLock::NewLock("ProgressHandlerMutex"))
, mProgressHandler(nsnull)
, mStorageService(aService)
{
  mFunctions.Init();
}

Connection::~Connection()
{
  (void)Close();
  nsAutoLock::DestroyLock(mAsyncExecutionMutex);
  nsAutoLock::DestroyLock(mTransactionMutex);
  nsAutoLock::DestroyLock(mFunctionsMutex);
  nsAutoLock::DestroyLock(mProgressHandlerMutex);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
  Connection,
  mozIStorageConnection
)

already_AddRefed<nsIEventTarget>
Connection::getAsyncExecutionTarget()
{
  nsAutoLock mutex(mAsyncExecutionMutex);

  // If we are shutting down the asynchronous thread, don't hand out any more
  // references to the thread.
  if (mAsyncExecutionThreadShuttingDown)
    return nsnull;

  if (!mAsyncExecutionThread) {
    nsresult rv = ::NS_NewThread(getter_AddRefs(mAsyncExecutionThread));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to create async thread.");
      return nsnull;
    }
  }

  nsIEventTarget *eventTarget;
  NS_ADDREF(eventTarget = mAsyncExecutionThread);
  return eventTarget;
}

nsresult
Connection::initialize(nsIFile *aDatabaseFile)
{
  NS_ASSERTION (!mDBConn, "Initialize called on already opened database!");
  NS_ENSURE_TRUE(mAsyncExecutionMutex, NS_ERROR_OUT_OF_MEMORY);
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

    srv = ::sqlite3_open(NS_ConvertUTF16toUTF8(path).get(), &mDBConn);
  }
  else {
    // in memory database requested, sqlite uses a magic file name
    srv = ::sqlite3_open(":memory:", &mDBConn);
  }
  if (srv != SQLITE_OK) {
    mDBConn = nsnull;
    return ConvertResultCode(srv);
  }

#ifdef PR_LOGGING
  if (!gStorageLog)
    gStorageLog = ::PR_NewLogModule("mozStorage");

  ::sqlite3_trace(mDBConn, tracefunc, this);

  nsCAutoString leafName(":memory");
  if (aDatabaseFile)
    (void)aDatabaseFile->GetNativeLeafName(leafName);
  PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Opening connection to '%s' (%p)",
                                      leafName.get(), this));
#endif

  // Register our built-in SQL functions.
  if (registerFunctions(mDBConn) != SQLITE_OK) {
    mDBConn = nsnull;
    return ConvertResultCode(srv);
  }

  // Execute a dummy statement to force the db open, and to verify if it is
  // valid or not.
  sqlite3_stmt *stmt;
  srv = ::sqlite3_prepare_v2(mDBConn, "SELECT * FROM sqlite_master", -1, &stmt,
                             NULL);
  if (srv == SQLITE_OK) {
    srv = ::sqlite3_step(stmt);

    if (srv == SQLITE_DONE || srv == SQLITE_ROW)
        srv = SQLITE_OK;
    ::sqlite3_finalize(stmt);
  }

  if (srv != SQLITE_OK) {
    ::sqlite3_close(mDBConn);
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

nsresult
Connection::databaseElementExists(enum DatabaseElementType aElementType,
                                  const nsACString &aElementName,
                                  PRBool *_exists)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  nsCAutoString query("SELECT name FROM sqlite_master WHERE type = '");
  switch (aElementType) {
    case INDEX:
      query.Append("index");
      break;
    case TABLE:
      query.Append("table");
      break;
  }
  query.Append("' AND name ='");
  query.Append(aElementName);
  query.Append("'");

  sqlite3_stmt *stmt;
  int srv = ::sqlite3_prepare_v2(mDBConn, query.get(), -1, &stmt, NULL);
  if (srv != SQLITE_OK)
      return ConvertResultCode(srv);

  srv = ::sqlite3_step(stmt);
  // we just care about the return value from step
  (void)::sqlite3_finalize(stmt);

  if (srv == SQLITE_ROW) {
    *_exists = PR_TRUE;
    return NS_OK;
  }
  if (srv == SQLITE_DONE) {
    *_exists = PR_FALSE;
    return NS_OK;
  }

  return ConvertResultCode(srv);
}

bool
Connection::findFunctionByInstance(nsISupports *aInstance)
{
  PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(mFunctionsMutex);
  FFEArguments args = { aInstance, false };
  mFunctions.EnumerateRead(findFunctionEnumerator, &args);
  return args.found;
}

/* static */ int
Connection::sProgressHelper(void *aArg)
{
  Connection *_this = static_cast<Connection *>(aArg);
  return _this->progressHandler();
}

int
Connection::progressHandler()
{
  nsAutoLock mutex(mProgressHandlerMutex);
  if (mProgressHandler) {
    PRBool result;
    nsresult rv = mProgressHandler->OnProgress(this, &result);
    if (NS_FAILED(rv)) return 0; // Don't break request
    return result ? 1 : 0;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageConnection

NS_IMETHODIMP
Connection::Close()
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

  // Flag that we are shutting down the async thread, so that
  // getAsyncExecutionTarget knows not to expose/create the async thread.
  {
    nsAutoLock mutex(mAsyncExecutionMutex);
    mAsyncExecutionThreadShuttingDown = PR_TRUE;
  }
  // Shutdown the async thread if it exists.  (Because we just set the flag,
  // we are the only code that is going to be touching this variable from here
  // on out.)
  if (mAsyncExecutionThread) {
    mAsyncExecutionThread->Shutdown();
    mAsyncExecutionThread = nsnull;
  }

#ifdef DEBUG
  // Notify about any non-finalized statements.
  sqlite3_stmt *stmt = NULL;
  while ((stmt = ::sqlite3_next_stmt(mDBConn, stmt))) {
    char *msg = ::PR_smprintf("SQL statement '%s' was not finalized",
                              ::sqlite3_sql(stmt));
    NS_WARNING(msg);
    ::PR_smprintf_free(msg);
  }
#endif

  {
    nsAutoLock mutex(mProgressHandlerMutex);
    if (mProgressHandler)
      ::sqlite3_progress_handler(mDBConn, 0, NULL, NULL);
  }

  int srv = ::sqlite3_close(mDBConn);
  if (srv != SQLITE_OK)
    NS_ERROR("sqlite3_close failed. There are probably outstanding statements that are listed above!");

  mDBConn = NULL;
  return ConvertResultCode(srv);
}

NS_IMETHODIMP
Connection::GetConnectionReady(PRBool *_ready)
{
  *_ready = (mDBConn != nsnull);
  return NS_OK;
}

NS_IMETHODIMP
Connection::GetDatabaseFile(nsIFile **_dbFile)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  NS_IF_ADDREF(*_dbFile = mDatabaseFile);

  return NS_OK;
}

NS_IMETHODIMP
Connection::GetLastInsertRowID(PRInt64 *_id)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  sqlite_int64 id = ::sqlite3_last_insert_rowid(mDBConn);
  *_id = id;

  return NS_OK;
}

NS_IMETHODIMP
Connection::GetLastError(PRInt32 *_error)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  *_error = ::sqlite3_errcode(mDBConn);

  return NS_OK;
}

NS_IMETHODIMP
Connection::GetLastErrorString(nsACString &_errorString)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  const char *serr = ::sqlite3_errmsg(mDBConn);
  _errorString.Assign(serr);

  return NS_OK;
}

NS_IMETHODIMP
Connection::GetSchemaVersion(PRInt32 *_version)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<mozIStorageStatement> stmt;
  (void)CreateStatement(NS_LITERAL_CSTRING("PRAGMA user_version"),
                        getter_AddRefs(stmt));
  NS_ENSURE_TRUE(stmt, NS_ERROR_OUT_OF_MEMORY);

  *_version = 0;
  PRBool hasResult;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult)
    *_version = stmt->AsInt32(0);

  return NS_OK;
}

NS_IMETHODIMP
Connection::SetSchemaVersion(PRInt32 aVersion)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  nsCAutoString stmt(NS_LITERAL_CSTRING("PRAGMA user_version = "));
  stmt.AppendInt(aVersion);

  return ExecuteSimpleSQL(stmt);
}

NS_IMETHODIMP
Connection::CreateStatement(const nsACString &aSQLStatement,
                            mozIStorageStatement **_stmt)
{
  NS_ENSURE_ARG_POINTER(_stmt);
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<mozStorageStatement> statement(new mozStorageStatement());
  NS_ENSURE_TRUE(statement, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = statement->Initialize(this, aSQLStatement);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatement *rawPtr;
  statement.forget(&rawPtr);
  *_stmt = rawPtr;
  return NS_OK;
}

NS_IMETHODIMP
Connection::ExecuteSimpleSQL(const nsACString &aSQLStatement)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_exec(mDBConn, PromiseFlatCString(aSQLStatement).get(),
                           NULL, NULL, NULL);
  return ConvertResultCode(srv);
}

nsresult
Connection::ExecuteAsync(mozIStorageStatement **aStatements,
                         PRUint32 aNumStatements,
                         mozIStorageStatementCallback *aCallback,
                         mozIStoragePendingStatement **_handle)
{
  int rc = SQLITE_OK;
  nsTArray<sqlite3_stmt *> stmts(aNumStatements);
  for (PRUint32 i = 0; i < aNumStatements && rc == SQLITE_OK; i++) {
    sqlite3_stmt *old_stmt =
        static_cast<mozStorageStatement *>(aStatements[i])->nativeStatement();
    if (!old_stmt) {
      rc = SQLITE_MISUSE;
      break;
    }
    NS_ASSERTION(::sqlite3_db_handle(old_stmt) == mDBConn,
                 "Statement must be from this database connection!");

    // Clone this statement.  We only need a sqlite3_stmt object, so we can
    // avoid all the extra work that making a new mozStorageStatement would
    // normally involve and use the SQLite API.
    sqlite3_stmt *new_stmt;
    rc = ::sqlite3_prepare_v2(mDBConn, ::sqlite3_sql(old_stmt), -1, &new_stmt,
                              NULL);
    if (rc != SQLITE_OK)
      break;

#ifdef PR_LOGGING
    PR_LOG(gStorageLog, PR_LOG_NOTICE,
           ("Cloned statement 0x%p to 0x%p", old_stmt, new_stmt));
#endif

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
    rv = AsyncExecuteStatements::execute(stmts, this, aCallback, _handle);

  // We had a failure, so we need to clean up...
  if (rc != SQLITE_OK || NS_FAILED(rv)) {
    for (PRUint32 i = 0; i < stmts.Length(); i++)
      (void)::sqlite3_finalize(stmts[i]);

    if (rc != SQLITE_OK)
      rv = ConvertResultCode(rc);
  }

  // Always reset all the statements
  for (PRUint32 i = 0; i < aNumStatements; i++)
    (void)aStatements[i]->Reset();

  return rv;
}

NS_IMETHODIMP
Connection::TableExists(const nsACString &aTableName,
                        PRBool *_exists)
{
    return databaseElementExists(TABLE, aTableName, _exists);
}

NS_IMETHODIMP
Connection::IndexExists(const nsACString &aIndexName,
                        PRBool* _exists)
{
    return databaseElementExists(INDEX, aIndexName, _exists);
}

NS_IMETHODIMP
Connection::GetTransactionInProgress(PRBool *_inProgress)
{
  nsAutoLock mutex(mTransactionMutex);
  *_inProgress = mTransactionInProgress;
  return NS_OK;
}

NS_IMETHODIMP
Connection::BeginTransaction()
{
  return BeginTransactionAs(mozIStorageConnection::TRANSACTION_DEFERRED);
}

NS_IMETHODIMP
Connection::BeginTransactionAs(PRInt32 aTransactionType)
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
Connection::CommitTransaction()
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
Connection::RollbackTransaction()
{
  nsAutoLock mutex(mTransactionMutex);
  if (!mTransactionInProgress)
    return NS_ERROR_FAILURE;
  nsresult rv = ExecuteSimpleSQL(NS_LITERAL_CSTRING("ROLLBACK TRANSACTION"));
  if (NS_SUCCEEDED(rv))
    mTransactionInProgress = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
Connection::CreateTable(const char *aTableName,
                        const char *aTableSchema)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  char *buf = ::PR_smprintf("CREATE TABLE %s (%s)", aTableName, aTableSchema);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;

  int srv = ::sqlite3_exec(mDBConn, buf, NULL, NULL, NULL);
  ::PR_smprintf_free(buf);

  return ConvertResultCode(srv);
}

NS_IMETHODIMP
Connection::CreateFunction(const nsACString &aFunctionName,
                           PRInt32 aNumArguments,
                           mozIStorageFunction *aFunction)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  // Check to see if this function is already defined.  We only check the name
  // because a function can be defined with the same body but different names.
  nsAutoLock mutex(mFunctionsMutex);
  NS_ENSURE_FALSE(mFunctions.Get(aFunctionName, NULL), NS_ERROR_FAILURE);

  int srv = ::sqlite3_create_function(mDBConn,
                                      nsPromiseFlatCString(aFunctionName).get(),
                                      aNumArguments,
                                      SQLITE_ANY,
                                      aFunction,
                                      basicFunctionHelper,
                                      NULL,
                                      NULL);
  if (srv != SQLITE_OK)
    return ConvertResultCode(srv);

  NS_ENSURE_TRUE(mFunctions.Put(aFunctionName, aFunction),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
Connection::CreateAggregateFunction(const nsACString &aFunctionName,
                                    PRInt32 aNumArguments,
                                    mozIStorageAggregateFunction *aFunction)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  // Check to see if this function name is already defined.
  nsAutoLock mutex(mFunctionsMutex);
  NS_ENSURE_FALSE(mFunctions.Get(aFunctionName, NULL), NS_ERROR_FAILURE);

  // Because aggregate functions depend on state across calls, you cannot have
  // the same instance use the same name.  We want to enumerate all functions
  // and make sure this instance is not already registered.
  NS_ENSURE_FALSE(findFunctionByInstance(aFunction), NS_ERROR_FAILURE);

  int srv = ::sqlite3_create_function(mDBConn,
                                      nsPromiseFlatCString(aFunctionName).get(),
                                      aNumArguments,
                                      SQLITE_ANY,
                                      aFunction,
                                      NULL,
                                      aggregateFunctionStepHelper,
                                      aggregateFunctionFinalHelper);
  if (srv != SQLITE_OK)
    return ConvertResultCode(srv);

  NS_ENSURE_TRUE(mFunctions.Put(aFunctionName, aFunction),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
Connection::RemoveFunction(const nsACString &aFunctionName)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  nsAutoLock mutex(mFunctionsMutex);
  NS_ENSURE_TRUE(mFunctions.Get(aFunctionName, NULL), NS_ERROR_FAILURE);

  int srv = ::sqlite3_create_function(mDBConn,
                                      nsPromiseFlatCString(aFunctionName).get(),
                                      0,
                                      SQLITE_ANY,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
  if (srv != SQLITE_OK)
    return ConvertResultCode(srv);

  mFunctions.Remove(aFunctionName);

  return NS_OK;
}

NS_IMETHODIMP
Connection::SetProgressHandler(PRInt32 aGranularity,
                               mozIStorageProgressHandler *aHandler,
                               mozIStorageProgressHandler **_oldHandler)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  // Return previous one
  nsAutoLock mutex(mProgressHandlerMutex);
  NS_IF_ADDREF(*_oldHandler = mProgressHandler);

  if (!aHandler || aGranularity <= 0) {
    aHandler = nsnull;
    aGranularity = 0;
  }
  mProgressHandler = aHandler;
  ::sqlite3_progress_handler(mDBConn, aGranularity, sProgressHelper, this);

  return NS_OK;
}

NS_IMETHODIMP
Connection::RemoveProgressHandler(mozIStorageProgressHandler **_oldHandler)
{
  if (!mDBConn) return NS_ERROR_NOT_INITIALIZED;

  // Return previous one
  nsAutoLock mutex(mProgressHandlerMutex);
  NS_IF_ADDREF(*_oldHandler = mProgressHandler);

  mProgressHandler = nsnull;
  ::sqlite3_progress_handler(mDBConn, 0, NULL, NULL);

  return NS_OK;
}

} // namespace storage
} // namespace mozilla
