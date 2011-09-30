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

#include "nsString.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageStatementWrapper.h"
#include "mozStorageStatementParams.h"
#include "mozStorageStatementRow.h"

#include "sqlite3.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// StatementWrapper

StatementWrapper::StatementWrapper()
: mStatement(nsnull)
{
}

StatementWrapper::~StatementWrapper()
{
  mStatement = nsnull;
}

NS_IMPL_ISUPPORTS2(
  StatementWrapper,
  mozIStorageStatementWrapper,
  nsIXPCScriptable
)

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageStatementWrapper

NS_IMETHODIMP
StatementWrapper::Initialize(mozIStorageStatement *aStatement)
{
  NS_ASSERTION(mStatement == nsnull, "StatementWrapper is already initialized");
  NS_ENSURE_ARG_POINTER(aStatement);

  mStatement = static_cast<Statement *>(aStatement);

  // fetch various things we care about
  (void)mStatement->GetParameterCount(&mParamCount);
  (void)mStatement->GetColumnCount(&mResultColumnCount);

  for (unsigned int i = 0; i < mResultColumnCount; i++) {
    const void *name = ::sqlite3_column_name16(nativeStatement(), i);
    (void)mColumnNames.AppendElement(nsDependentString(static_cast<const PRUnichar*>(name)));
  }

  return NS_OK;
}

NS_IMETHODIMP
StatementWrapper::GetStatement(mozIStorageStatement **_statement)
{
  NS_IF_ADDREF(*_statement = mStatement);
  return NS_OK;
}

NS_IMETHODIMP
StatementWrapper::Reset()
{
  if (!mStatement)
    return NS_ERROR_FAILURE;

  return mStatement->Reset();
}

NS_IMETHODIMP
StatementWrapper::Step(bool *_hasMoreResults)
{
  if (!mStatement)
    return NS_ERROR_FAILURE;

  bool hasMore = false;
  nsresult rv = mStatement->ExecuteStep(&hasMore);
  if (NS_SUCCEEDED(rv) && !hasMore) {
    *_hasMoreResults = PR_FALSE;
    (void)mStatement->Reset();
    return NS_OK;
  }

  *_hasMoreResults = hasMore;
  return rv;
}

NS_IMETHODIMP
StatementWrapper::Execute()
{
  if (!mStatement)
    return NS_ERROR_FAILURE;

  return mStatement->Execute();
}

NS_IMETHODIMP
StatementWrapper::GetRow(mozIStorageStatementRow **_row)
{
  NS_ENSURE_ARG_POINTER(_row);

  if (!mStatement)
    return NS_ERROR_FAILURE;

  PRInt32 state;
  mStatement->GetState(&state);
  if (state != mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING)
    return NS_ERROR_FAILURE;

  if (!mStatementRow) {
    mStatementRow = new StatementRow(mStatement);
    NS_ENSURE_TRUE(mStatementRow, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*_row = mStatementRow);
  return NS_OK;
}

NS_IMETHODIMP
StatementWrapper::GetParams(mozIStorageStatementParams **_params)
{
  NS_ENSURE_ARG_POINTER(_params);

  if (!mStatementParams) {
    mStatementParams = new StatementParams(mStatement);
    NS_ENSURE_TRUE(mStatementParams, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*_params = mStatementParams);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME StatementWrapper
#define XPC_MAP_QUOTED_CLASSNAME "StatementWrapper"
#define XPC_MAP_WANT_CALL
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE | \
                      nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY
#include "xpc_map_end.h"

NS_IMETHODIMP
StatementWrapper::Call(nsIXPConnectWrappedNative *aWrapper,
                       JSContext *aCtx,
                       JSObject *aScopeObj,
                       PRUint32 aArgc,
                       jsval *aArgv,
                       jsval *_vp,
                       bool *_retval)
{
  if (!mStatement)
    return NS_ERROR_FAILURE;

  if (aArgc != mParamCount) {
    *_retval = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  // reset
  (void)mStatement->Reset();

  // bind parameters
  for (int i = 0; i < (int)aArgc; i++) {
    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, aArgv[i]));
    if (!variant ||
        NS_FAILED(mStatement->BindByIndex(i, variant))) {
      *_retval = PR_FALSE;
      return NS_ERROR_INVALID_ARG;
    }
  }

  // if there are no results, we just execute
  if (mResultColumnCount == 0)
    (void)mStatement->Execute();

  *_vp = JSVAL_TRUE;
  *_retval = PR_TRUE;
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
