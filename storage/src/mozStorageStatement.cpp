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
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   John Zhang <jzhang@aptana.com>
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

#include <limits.h>
#include <stdio.h>

#include "nsError.h"
#include "nsMemory.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"
#include "Variant.h"

#include "mozIStorageError.h"

#include "mozStorageBindingParams.h"
#include "mozStorageConnection.h"
#include "mozStorageStatementJSHelper.h"
#include "mozStoragePrivateHelpers.h"
#include "mozStorageStatementParams.h"
#include "mozStorageStatementRow.h"
#include "mozStorageStatement.h"

#include "prlog.h"

#include "mozilla/FunctionTimer.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gStorageLog;
#endif

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// nsIClassInfo

NS_IMPL_CI_INTERFACE_GETTER5(
  Statement,
  mozIStorageStatement,
  mozIStorageBaseStatement,
  mozIStorageBindingParams,
  mozIStorageValueArray,
  mozilla::storage::StorageBaseStatementInternal
)

class StatementClassInfo : public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHODIMP
  GetInterfaces(PRUint32 *_count, nsIID ***_array)
  {
    return NS_CI_INTERFACE_GETTER_NAME(Statement)(_count, _array);
  }

  NS_IMETHODIMP
  GetHelperForLanguage(PRUint32 aLanguage, nsISupports **_helper)
  {
    if (aLanguage == nsIProgrammingLanguage::JAVASCRIPT) {
      static StatementJSHelper sJSHelper;
      *_helper = &sJSHelper;
      return NS_OK;
    }

    *_helper = nsnull;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetContractID(char **_contractID)
  {
    *_contractID = nsnull;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetClassDescription(char **_desc)
  {
    *_desc = nsnull;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetClassID(nsCID **_id)
  {
    *_id = nsnull;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetImplementationLanguage(PRUint32 *_language)
  {
    *_language = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetFlags(PRUint32 *_flags)
  {
    *_flags = nsnull;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetClassIDNoAlloc(nsCID *_cid)
  {
    return NS_ERROR_NOT_AVAILABLE;
  }
};

NS_IMETHODIMP_(nsrefcnt) StatementClassInfo::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) StatementClassInfo::Release() { return 1; }
NS_IMPL_QUERY_INTERFACE1(StatementClassInfo, nsIClassInfo)

static StatementClassInfo sStatementClassInfo;

////////////////////////////////////////////////////////////////////////////////
//// Statement

Statement::Statement()
: StorageBaseStatementInternal()
, mDBStatement(NULL)
, mColumnNames()
, mExecuting(false)
{
}

nsresult
Statement::initialize(Connection *aDBConnection,
                      const nsACString &aSQLStatement)
{
  NS_ASSERTION(aDBConnection, "No database connection given!");
  NS_ASSERTION(!mDBStatement, "Statement already initialized!");

  sqlite3 *db = aDBConnection->GetNativeConnection();
  NS_ASSERTION(db, "We should never be called with a null sqlite3 database!");

  int srv = prepareStmt(db, PromiseFlatCString(aSQLStatement), &mDBStatement);
  if (srv != SQLITE_OK) {
#ifdef PR_LOGGING
      PR_LOG(gStorageLog, PR_LOG_ERROR,
             ("Sqlite statement prepare error: %d '%s'", srv,
              ::sqlite3_errmsg(db)));
      PR_LOG(gStorageLog, PR_LOG_ERROR,
             ("Statement was: '%s'", PromiseFlatCString(aSQLStatement).get()));
#endif
      return NS_ERROR_FAILURE;
    }

#ifdef PR_LOGGING
  PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Initialized statement '%s' (0x%p)",
                                      PromiseFlatCString(aSQLStatement).get(),
                                      mDBStatement));
#endif

  mDBConnection = aDBConnection;
  mParamCount = ::sqlite3_bind_parameter_count(mDBStatement);
  mResultColumnCount = ::sqlite3_column_count(mDBStatement);
  mColumnNames.Clear();

  for (PRUint32 i = 0; i < mResultColumnCount; i++) {
      const char *name = ::sqlite3_column_name(mDBStatement, i);
      (void)mColumnNames.AppendElement(nsDependentCString(name));
  }

#ifdef DEBUG
  // We want to try and test for LIKE and that consumers are using
  // escapeStringForLIKE instead of just trusting user input.  The idea to
  // check to see if they are binding a parameter after like instead of just
  // using a string.  We only do this in debug builds because it's expensive!
  const nsCaseInsensitiveCStringComparator c;
  nsACString::const_iterator start, end, e;
  aSQLStatement.BeginReading(start);
  aSQLStatement.EndReading(end);
  e = end;
  while (::FindInReadable(NS_LITERAL_CSTRING(" LIKE"), start, e, c)) {
    // We have a LIKE in here, so we perform our tests
    // FindInReadable moves the iterator, so we have to get a new one for
    // each test we perform.
    nsACString::const_iterator s1, s2, s3;
    s1 = s2 = s3 = start;

    if (!(::FindInReadable(NS_LITERAL_CSTRING(" LIKE ?"), s1, end, c) ||
          ::FindInReadable(NS_LITERAL_CSTRING(" LIKE :"), s2, end, c) ||
          ::FindInReadable(NS_LITERAL_CSTRING(" LIKE @"), s3, end, c))) {
      // At this point, we didn't find a LIKE statement followed by ?, :,
      // or @, all of which are valid characters for binding a parameter.
      // We will warn the consumer that they may not be safely using LIKE.
      NS_WARNING("Unsafe use of LIKE detected!  Please ensure that you "
                 "are using mozIStorageStatement::escapeStringForLIKE "
                 "and that you are binding that result to the statement "
                 "to prevent SQL injection attacks.");
    }

    // resetting start and e
    start = e;
    e = end;
  }
#endif

  return NS_OK;
}

mozIStorageBindingParams *
Statement::getParams()
{
  nsresult rv;

  // If we do not have an array object yet, make it.
  if (!mParamsArray) {
    nsCOMPtr<mozIStorageBindingParamsArray> array;
    rv = NewBindingParamsArray(getter_AddRefs(array));
    NS_ENSURE_SUCCESS(rv, nsnull);

    mParamsArray = static_cast<BindingParamsArray *>(array.get());
  }

  // If there isn't already any rows added, we'll have to add one to use.
  if (mParamsArray->length() == 0) {
    nsRefPtr<BindingParams> params(new BindingParams(mParamsArray, this));
    NS_ENSURE_TRUE(params, nsnull);

    rv = mParamsArray->AddParams(params);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // We have to unlock our params because AddParams locks them.  This is safe
    // because no reference to the params object was, or ever will be given out.
    params->unlock(this);

    // We also want to lock our array at this point - we don't want anything to
    // be added to it.  Nothing has, or will ever get a reference to it, but we
    // will get additional safety checks via assertions by doing this.
    mParamsArray->lock();
  }

  return *mParamsArray->begin();
}

Statement::~Statement()
{
  (void)internalFinalize(true);
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_THREADSAFE_ADDREF(Statement)
NS_IMPL_THREADSAFE_RELEASE(Statement)

NS_INTERFACE_MAP_BEGIN(Statement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageStatement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageBaseStatement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageBindingParams)
  NS_INTERFACE_MAP_ENTRY(mozIStorageValueArray)
  NS_INTERFACE_MAP_ENTRY(mozilla::storage::StorageBaseStatementInternal)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    foundInterface = static_cast<nsIClassInfo *>(&sStatementClassInfo);
  }
  else
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozIStorageStatement)
NS_INTERFACE_MAP_END


////////////////////////////////////////////////////////////////////////////////
//// StorageBaseStatementInternal

Connection *
Statement::getOwner()
{
  return mDBConnection;
}

int
Statement::getAsyncStatement(sqlite3_stmt **_stmt)
{
  // If we have no statement, we shouldn't be calling this method!
  NS_ASSERTION(mDBStatement != NULL, "We have no statement to clone!");

  // If we do not yet have a cached async statement, clone our statement now.
  if (!mAsyncStatement) {
    nsDependentCString sql(::sqlite3_sql(mDBStatement));
    int rc = prepareStmt(mDBConnection->GetNativeConnection(), sql,
                     &mAsyncStatement);
    if (rc != SQLITE_OK) {
      *_stmt = nsnull;
      return rc;
    }

#ifdef PR_LOGGING
    PR_LOG(gStorageLog, PR_LOG_NOTICE,
           ("Cloned statement 0x%p to 0x%p", mDBStatement, mAsyncStatement));
#endif
  }

  *_stmt = mAsyncStatement;
  return SQLITE_OK;
}

nsresult
Statement::getAsynchronousStatementData(StatementData &_data)
{
  if (!mDBStatement)
    return NS_ERROR_UNEXPECTED;

  sqlite3_stmt *stmt;
  int rc = getAsyncStatement(&stmt);
  if (rc != SQLITE_OK)
    return convertResultCode(rc);

  _data = StatementData(stmt, bindingParamsArray(), this);

  return NS_OK;
}

already_AddRefed<mozIStorageBindingParams>
Statement::newBindingParams(mozIStorageBindingParamsArray *aOwner)
{
  nsCOMPtr<mozIStorageBindingParams> params = new BindingParams(aOwner, this);
  return params.forget();
}


////////////////////////////////////////////////////////////////////////////////
//// mozIStorageStatement

// proxy to StorageBaseStatementInternal using its define helper.
MIXIN_IMPL_STORAGEBASESTATEMENTINTERNAL(Statement, (void)0;)

NS_IMETHODIMP
Statement::Clone(mozIStorageStatement **_statement)
{
  nsRefPtr<Statement> statement(new Statement());
  NS_ENSURE_TRUE(statement, NS_ERROR_OUT_OF_MEMORY);

  nsCAutoString sql(::sqlite3_sql(mDBStatement));
  nsresult rv = statement->initialize(mDBConnection, sql);
  NS_ENSURE_SUCCESS(rv, rv);

  statement.forget(_statement);
  return NS_OK;
}

NS_IMETHODIMP
Statement::Finalize()
{
  return internalFinalize(false);
}

nsresult
Statement::internalFinalize(bool aDestructing)
{
  if (!mDBStatement)
    return NS_OK;

#ifdef PR_LOGGING
  PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Finalizing statement '%s'",
                                      ::sqlite3_sql(mDBStatement)));
#endif

  int srv = ::sqlite3_finalize(mDBStatement);
  mDBStatement = NULL;

  if (mAsyncStatement) {
    // If the destructor called us, there are no pending async statements (they
    // hold a reference to us) and we can/must just kill the statement directly.
    if (aDestructing)
      destructorAsyncFinalize();
    else
      asyncFinalize();
  }

  // We are considered dead at this point, so any wrappers for row or params
  // need to lose their reference to us.
  if (mStatementParamsHolder) {
    nsCOMPtr<nsIXPConnectWrappedNative> wrapper =
        do_QueryInterface(mStatementParamsHolder);
    nsCOMPtr<mozIStorageStatementParams> iParams =
        do_QueryWrappedNative(wrapper);
    StatementParams *params = static_cast<StatementParams *>(iParams.get());
    params->mStatement = nsnull;
    mStatementParamsHolder = nsnull;
  }

  if (mStatementRowHolder) {
    nsCOMPtr<nsIXPConnectWrappedNative> wrapper =
        do_QueryInterface(mStatementRowHolder);
    nsCOMPtr<mozIStorageStatementRow> iRow =
        do_QueryWrappedNative(wrapper);
    StatementRow *row = static_cast<StatementRow *>(iRow.get());
    row->mStatement = nsnull;
    mStatementRowHolder = nsnull;
  }

  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::GetParameterCount(PRUint32 *_parameterCount)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  *_parameterCount = mParamCount;
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetParameterName(PRUint32 aParamIndex,
                            nsACString &_name)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;
  ENSURE_INDEX_VALUE(aParamIndex, mParamCount);

  const char *name = ::sqlite3_bind_parameter_name(mDBStatement,
                                                   aParamIndex + 1);
  if (name == NULL) {
    // this thing had no name, so fake one
    nsCAutoString name(":");
    name.AppendInt(aParamIndex);
    _name.Assign(name);
  }
  else {
    _name.Assign(nsDependentCString(name));
  }

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetParameterIndex(const nsACString &aName,
                             PRUint32 *_index)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  // We do not accept any forms of names other than ":name", but we need to add
  // the colon for SQLite.
  nsCAutoString name(":");
  name.Append(aName);
  int ind = ::sqlite3_bind_parameter_index(mDBStatement, name.get());
  if (ind  == 0) // Named parameter not found.
    return NS_ERROR_INVALID_ARG;

  *_index = ind - 1; // SQLite indexes are 1-based, we are 0-based.

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetColumnCount(PRUint32 *_columnCount)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  *_columnCount = mResultColumnCount;
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetColumnName(PRUint32 aColumnIndex,
                         nsACString &_name)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;
  ENSURE_INDEX_VALUE(aColumnIndex, mResultColumnCount);

  const char *cname = ::sqlite3_column_name(mDBStatement, aColumnIndex);
  _name.Assign(nsDependentCString(cname));

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetColumnIndex(const nsACString &aName,
                          PRUint32 *_index)
{
  if (!mDBStatement)
      return NS_ERROR_NOT_INITIALIZED;

  // Surprisingly enough, SQLite doesn't provide an API for this.  We have to
  // determine it ourselves sadly.
  for (PRUint32 i = 0; i < mResultColumnCount; i++) {
    if (mColumnNames[i].Equals(aName)) {
      *_index = i;
      return NS_OK;
    }
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
Statement::Reset()
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

#ifdef DEBUG
  PR_LOG(gStorageLog, PR_LOG_DEBUG, ("Resetting statement: '%s'",
                                     ::sqlite3_sql(mDBStatement)));

  checkAndLogStatementPerformance(mDBStatement);
#endif

  mParamsArray = nsnull;
  (void)sqlite3_reset(mDBStatement);
  (void)sqlite3_clear_bindings(mDBStatement);

  mExecuting = false;

  return NS_OK;
}

NS_IMETHODIMP
Statement::BindParameters(mozIStorageBindingParamsArray *aParameters)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  BindingParamsArray *array = static_cast<BindingParamsArray *>(aParameters);
  if (array->getOwner() != this)
    return NS_ERROR_UNEXPECTED;

  if (array->length() == 0)
    return NS_ERROR_UNEXPECTED;

  mParamsArray = array;
  mParamsArray->lock();

  return NS_OK;
}

NS_IMETHODIMP
Statement::Execute()
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  PRBool ret;
  nsresult rv = ExecuteStep(&ret);
  nsresult rv2 = Reset();

  return NS_FAILED(rv) ? rv : rv2;
}

NS_IMETHODIMP
Statement::ExecuteStep(PRBool *_moreResults)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  NS_TIME_FUNCTION_MIN_FMT(5, "mozIStorageStatement::ExecuteStep(%s) (0x%p)",
                           mDBConnection->getFilename().get(), mDBStatement);

  // Bind any parameters first before executing.
  if (mParamsArray) {
    // If we have more than one row of parameters to bind, they shouldn't be
    // calling this method (and instead use executeAsync).
    if (mParamsArray->length() != 1)
      return NS_ERROR_UNEXPECTED;

    BindingParamsArray::iterator row = mParamsArray->begin();
    nsCOMPtr<IStorageBindingParamsInternal> bindingInternal = 
      do_QueryInterface(*row);
    nsCOMPtr<mozIStorageError> error = bindingInternal->bind(mDBStatement);
    if (error) {
      PRInt32 srv;
      (void)error->GetResult(&srv);
      return convertResultCode(srv);
    }

    // We have bound, so now we can clear our array.
    mParamsArray = nsnull;
  }
  int srv = stepStmt(mDBStatement);

#ifdef PR_LOGGING
  if (srv != SQLITE_ROW && srv != SQLITE_DONE) {
      nsCAutoString errStr;
      (void)mDBConnection->GetLastErrorString(errStr);
      PR_LOG(gStorageLog, PR_LOG_DEBUG,
             ("Statement::ExecuteStep error: %s", errStr.get()));
  }
#endif

  // SQLITE_ROW and SQLITE_DONE are non-errors
  if (srv == SQLITE_ROW) {
    // we got a row back
    mExecuting = true;
    *_moreResults = PR_TRUE;
    return NS_OK;
  }
  else if (srv == SQLITE_DONE) {
    // statement is done (no row returned)
    mExecuting = false;
    *_moreResults = PR_FALSE;
    return NS_OK;
  }
  else if (srv == SQLITE_BUSY || srv == SQLITE_MISUSE) {
    mExecuting = PR_FALSE;
  }
  else if (mExecuting) {
#ifdef PR_LOGGING
    PR_LOG(gStorageLog, PR_LOG_ERROR,
           ("SQLite error after mExecuting was true!"));
#endif
    mExecuting = PR_FALSE;
  }

  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::GetState(PRInt32 *_state)
{
  if (!mDBStatement)
    *_state = MOZ_STORAGE_STATEMENT_INVALID;
  else if (mExecuting)
    *_state = MOZ_STORAGE_STATEMENT_EXECUTING;
  else
    *_state = MOZ_STORAGE_STATEMENT_READY;

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetColumnDecltype(PRUint32 aParamIndex,
                             nsACString &_declType)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  ENSURE_INDEX_VALUE(aParamIndex, mResultColumnCount);

  _declType.Assign(::sqlite3_column_decltype(mDBStatement, aParamIndex));
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageValueArray (now part of mozIStorageStatement too)

NS_IMETHODIMP
Statement::GetNumEntries(PRUint32 *_length)
{
  *_length = mResultColumnCount;
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetTypeOfIndex(PRUint32 aIndex,
                          PRInt32 *_type)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  ENSURE_INDEX_VALUE(aIndex, mResultColumnCount);

  if (!mExecuting)
    return NS_ERROR_UNEXPECTED;

  int t = ::sqlite3_column_type(mDBStatement, aIndex);
  switch (t) {
    case SQLITE_INTEGER:
      *_type = mozIStorageStatement::VALUE_TYPE_INTEGER;
      break;
    case SQLITE_FLOAT:
      *_type = mozIStorageStatement::VALUE_TYPE_FLOAT;
      break;
    case SQLITE_TEXT:
      *_type = mozIStorageStatement::VALUE_TYPE_TEXT;
      break;
    case SQLITE_BLOB:
      *_type = mozIStorageStatement::VALUE_TYPE_BLOB;
      break;
    case SQLITE_NULL:
      *_type = mozIStorageStatement::VALUE_TYPE_NULL;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetInt32(PRUint32 aIndex,
                    PRInt32 *_value)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  ENSURE_INDEX_VALUE(aIndex, mResultColumnCount);

  if (!mExecuting)
    return NS_ERROR_UNEXPECTED;

  *_value = ::sqlite3_column_int(mDBStatement, aIndex);
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetInt64(PRUint32 aIndex,
                    PRInt64 *_value)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  ENSURE_INDEX_VALUE(aIndex, mResultColumnCount);

  if (!mExecuting)
    return NS_ERROR_UNEXPECTED;

  *_value = ::sqlite3_column_int64(mDBStatement, aIndex);

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetDouble(PRUint32 aIndex,
                     double *_value)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  ENSURE_INDEX_VALUE(aIndex, mResultColumnCount);

  if (!mExecuting)
    return NS_ERROR_UNEXPECTED;

  *_value = ::sqlite3_column_double(mDBStatement, aIndex);

  return NS_OK;
}

NS_IMETHODIMP
Statement::GetUTF8String(PRUint32 aIndex,
                         nsACString &_value)
{
  // Get type of Index will check aIndex for us, so we don't have to.
  PRInt32 type;
  nsresult rv = GetTypeOfIndex(aIndex, &type);
  NS_ENSURE_SUCCESS(rv, rv);
  if (type == mozIStorageStatement::VALUE_TYPE_NULL) {
    // NULL columns should have IsVoid set to distinguish them from the empty
    // string.
    _value.Truncate(0);
    _value.SetIsVoid(PR_TRUE);
  }
  else {
    const char *value =
      reinterpret_cast<const char *>(::sqlite3_column_text(mDBStatement,
                                                           aIndex));
    _value.Assign(value, ::sqlite3_column_bytes(mDBStatement, aIndex));
  }
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetString(PRUint32 aIndex,
                     nsAString &_value)
{
  // Get type of Index will check aIndex for us, so we don't have to.
  PRInt32 type;
  nsresult rv = GetTypeOfIndex(aIndex, &type);
  NS_ENSURE_SUCCESS(rv, rv);
  if (type == mozIStorageStatement::VALUE_TYPE_NULL) {
    // NULL columns should have IsVoid set to distinguish them from the empty
    // string.
    _value.Truncate(0);
    _value.SetIsVoid(PR_TRUE);
  } else {
    const PRUnichar *value =
      static_cast<const PRUnichar *>(::sqlite3_column_text16(mDBStatement,
                                                             aIndex));
    _value.Assign(value, ::sqlite3_column_bytes16(mDBStatement, aIndex) / 2);
  }
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetBlob(PRUint32 aIndex,
                   PRUint32 *_size,
                   PRUint8 **_blob)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  ENSURE_INDEX_VALUE(aIndex, mResultColumnCount);

  if (!mExecuting)
     return NS_ERROR_UNEXPECTED;

  int size = ::sqlite3_column_bytes(mDBStatement, aIndex);
  void *blob = nsnull;
  if (size) {
    blob = nsMemory::Clone(::sqlite3_column_blob(mDBStatement, aIndex), size);
    NS_ENSURE_TRUE(blob, NS_ERROR_OUT_OF_MEMORY);
  }

  *_blob = static_cast<PRUint8 *>(blob);
  *_size = size;
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetSharedUTF8String(PRUint32 aIndex,
                               PRUint32 *_length,
                               const char **_value)
{
  if (_length)
    *_length = ::sqlite3_column_bytes(mDBStatement, aIndex);

  *_value = reinterpret_cast<const char *>(::sqlite3_column_text(mDBStatement,
                                                                 aIndex));
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetSharedString(PRUint32 aIndex,
                           PRUint32 *_length,
                           const PRUnichar **_value)
{
  if (_length)
    *_length = ::sqlite3_column_bytes16(mDBStatement, aIndex);

  *_value = static_cast<const PRUnichar *>(::sqlite3_column_text16(mDBStatement,
                                                                   aIndex));
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetSharedBlob(PRUint32 aIndex,
                         PRUint32 *_size,
                         const PRUint8 **_blob)
{
  *_size = ::sqlite3_column_bytes(mDBStatement, aIndex);
  *_blob = static_cast<const PRUint8 *>(::sqlite3_column_blob(mDBStatement,
                                                              aIndex));
  return NS_OK;
}

NS_IMETHODIMP
Statement::GetIsNull(PRUint32 aIndex,
                     PRBool *_isNull)
{
  // Get type of Index will check aIndex for us, so we don't have to.
  PRInt32 type;
  nsresult rv = GetTypeOfIndex(aIndex, &type);
  NS_ENSURE_SUCCESS(rv, rv);
  *_isNull = (type == mozIStorageStatement::VALUE_TYPE_NULL);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageBindingParams

BOILERPLATE_BIND_PROXIES(
  Statement, 
  if (!mDBStatement) return NS_ERROR_NOT_INITIALIZED;
)

} // namespace storage
} // namespace mozilla
