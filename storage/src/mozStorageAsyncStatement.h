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
 *   Andrew Sutherland <asutherland@asutherland.org>
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

#ifndef mozilla_storage_mozStorageAsyncStatement_h_
#define mozilla_storage_mozStorageAsyncStatement_h_

#include "nsAutoPtr.h"
#include "nsString.h"

#include "nsTArray.h"

#include "mozStorageBindingParamsArray.h"
#include "mozStorageStatementData.h"
#include "mozIStorageAsyncStatement.h"
#include "StorageBaseStatementInternal.h"

class nsIXPConnectJSObjectHolder;
struct sqlite3_stmt;

namespace mozilla {
namespace storage {

class AsyncStatementJSHelper;
class Connection;

class AsyncStatement : public mozIStorageAsyncStatement
                     , public StorageBaseStatementInternal
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEASYNCSTATEMENT
  NS_DECL_MOZISTORAGEBASESTATEMENT
  NS_DECL_MOZISTORAGEBINDINGPARAMS
  NS_DECL_STORAGEBASESTATEMENTINTERNAL

  AsyncStatement();

  /**
   * Initializes the object on aDBConnection by preparing the SQL statement
   * given by aSQLStatement.
   *
   * @param aDBConnection
   *        The Connection object this statement is associated with.
   * @param aSQLStatement
   *        The SQL statement to prepare that this object will represent.
   */
  nsresult initialize(Connection *aDBConnection,
                      const nsACString &aSQLStatement);

  /**
   * Obtains and transfers ownership of the array of parameters that are bound
   * to this statment.  This can be null.
   */
  inline already_AddRefed<BindingParamsArray> bindingParamsArray()
  {
    return mParamsArray.forget();
  }


private:
  ~AsyncStatement();

  /**
   * Clean up the references JS helpers hold to us.  For cycle-avoidance reasons
   * they do not hold reference-counted references to us, so it is important
   * we do this.
   */
  void cleanupJSHelpers();

  /**
   * @return a pointer to the BindingParams object to use with our Bind*
   *         method.
   */
  mozIStorageBindingParams *getParams();

  /**
   * The SQL string as passed by the user.  We store it because we create the
   * async statement on-demand on the async thread.
   */
  nsCString mSQLString;

  /**
   * Holds the array of parameters to bind to this statement when we execute
   * it asynchronously.
   */
  nsRefPtr<BindingParamsArray> mParamsArray;

  /**
   * Caches the JS 'params' helper for this statement.
   */
  nsCOMPtr<nsIXPConnectJSObjectHolder> mStatementParamsHolder;

  /**
   * Have we been explicitly finalized by the user?
   */
  bool mFinalized;

  /**
   * Required for access to private mStatementParamsHolder field by
   * AsyncStatementJSHelper::getParams.
   */
  friend class AsyncStatementJSHelper;
};

} // storage
} // mozilla

#endif // mozilla_storage_mozStorageAsyncStatement_h_
