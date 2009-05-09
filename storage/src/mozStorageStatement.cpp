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

#include <stdio.h>

#include "nsError.h"
#include "nsMemory.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"

#include "mozStorageConnection.h"
#include "mozStorageStatementJSHelper.h"
#include "mozStoragePrivateHelpers.h"
#include "mozStorageStatementParams.h"
#include "mozStorageStatementRow.h"
#include "mozStorageStatement.h"

#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gStorageLog;
#endif

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// nsIClassInfo

NS_IMPL_CI_INTERFACE_GETTER2(
  Statement,
  mozIStorageStatement,
  mozIStorageValueArray
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
: mDBConnection(nsnull)
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

  int srv = ::sqlite3_prepare_v2(db, PromiseFlatCString(aSQLStatement).get(),
                                 -1, &mDBStatement, NULL);
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
                 "are using mozIStorageConnection::escapeStringForLIKE "
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

Statement::~Statement()
{
  (void)Finalize();
}

NS_IMPL_THREADSAFE_ADDREF(Statement)
NS_IMPL_THREADSAFE_RELEASE(Statement)

NS_INTERFACE_MAP_BEGIN(Statement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageStatement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageValueArray)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    foundInterface = static_cast<nsIClassInfo *>(&sStatementClassInfo);
  }
  else
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageStatement

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
  if (!mDBStatement)
    return NS_OK;

#ifdef PR_LOGGING
  PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Finalizing statement '%s'",
                                      ::sqlite3_sql(mDBStatement)));
#endif

  int srv = ::sqlite3_finalize(mDBStatement);
  mDBStatement = NULL;

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

  int ind = ::sqlite3_bind_parameter_index(mDBStatement,
                                           PromiseFlatCString(aName).get());
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

  (void)sqlite3_reset(mDBStatement);
  (void)sqlite3_clear_bindings(mDBStatement);

  mExecuting = false;

  return NS_OK;
}

NS_IMETHODIMP
Statement::BindUTF8StringParameter(PRUint32 aParamIndex,
                                   const nsACString &aValue)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_text(mDBStatement, aParamIndex + 1,
                                PromiseFlatCString(aValue).get(),
                                aValue.Length(), SQLITE_TRANSIENT);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::BindStringParameter(PRUint32 aParamIndex,
                               const nsAString &aValue)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_text16(mDBStatement, aParamIndex + 1,
                                  PromiseFlatString(aValue).get(),
                                  aValue.Length() * 2, SQLITE_TRANSIENT);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::BindDoubleParameter(PRUint32 aParamIndex,
                               double aValue)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_double(mDBStatement, aParamIndex + 1, aValue);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::BindInt32Parameter(PRUint32 aParamIndex,
                              PRInt32 aValue)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_int(mDBStatement, aParamIndex + 1, aValue);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::BindInt64Parameter(PRUint32 aParamIndex,
                              PRInt64 aValue)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_int64(mDBStatement, aParamIndex + 1, aValue);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::BindNullParameter(PRUint32 aParamIndex)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_null(mDBStatement, aParamIndex + 1);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::BindBlobParameter(PRUint32 aParamIndex,
                             const PRUint8 *aValue,
                             PRUint32 aValueSize)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_bind_blob(mDBStatement, aParamIndex + 1, aValue,
                                aValueSize, SQLITE_TRANSIENT);
  return convertResultCode(srv);
}

NS_IMETHODIMP
Statement::Execute()
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  PRBool ret;
  nsresult rv = ExecuteStep(&ret);
  NS_ENSURE_SUCCESS(rv, rv);

  return Reset();
}

NS_IMETHODIMP
Statement::ExecuteStep(PRBool *_moreResults)
{
  if (!mDBStatement)
    return NS_ERROR_NOT_INITIALIZED;

  int srv = ::sqlite3_step(mDBStatement);

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

nsresult
Statement::ExecuteAsync(mozIStorageStatementCallback *aCallback,
                        mozIStoragePendingStatement **_stmt)
{
  mozIStorageStatement *stmts[1] = {this};
  return mDBConnection->ExecuteAsync(stmts, 1, aCallback, _stmt);
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
Statement::EscapeStringForLIKE(const nsAString &aValue,
                               const PRUnichar aEscapeChar,
                               nsAString &_escapedString)
{
  const PRUnichar MATCH_ALL('%');
  const PRUnichar MATCH_ONE('_');

  _escapedString.Truncate(0);

  for (PRInt32 i = 0; i < aValue.Length(); i++) {
    if (aValue[i] == aEscapeChar || aValue[i] == MATCH_ALL ||
        aValue[i] == MATCH_ONE)
      _escapedString += aEscapeChar;
    _escapedString += aValue[i];
  }
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
//// mozIStorageValueArray

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
      *_type = VALUE_TYPE_INTEGER;
      break;
    case SQLITE_FLOAT:
      *_type = VALUE_TYPE_FLOAT;
      break;
    case SQLITE_TEXT:
      *_type = VALUE_TYPE_TEXT;
      break;
    case SQLITE_BLOB:
      *_type = VALUE_TYPE_BLOB;
      break;
    case SQLITE_NULL:
      *_type = VALUE_TYPE_NULL;
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
  if (type == VALUE_TYPE_NULL) {
    // NULL columns should have IsVod set to distinguis them from an empty
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
  if (type == VALUE_TYPE_NULL) {
    // NULL columns should have IsVod set to distinguis them from an empty
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
  *_isNull = (type == VALUE_TYPE_NULL);
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
