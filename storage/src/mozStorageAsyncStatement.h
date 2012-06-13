/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_mozStorageAsyncStatement_h_
#define mozilla_storage_mozStorageAsyncStatement_h_

#include "nsAutoPtr.h"
#include "nsString.h"

#include "nsTArray.h"

#include "mozStorageBindingParamsArray.h"
#include "mozStorageStatementData.h"
#include "mozIStorageAsyncStatement.h"
#include "StorageBaseStatementInternal.h"
#include "mozilla/Attributes.h"

class nsIXPConnectJSObjectHolder;
struct sqlite3_stmt;

namespace mozilla {
namespace storage {

class AsyncStatementJSHelper;
class Connection;

class AsyncStatement MOZ_FINAL : public mozIStorageAsyncStatement
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
