/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageStatement_h
#define mozStorageStatement_h

#include "nsAutoPtr.h"
#include "nsString.h"

#include "nsTArray.h"

#include "mozStorageBindingParamsArray.h"
#include "mozStorageStatementData.h"
#include "mozIStorageStatement.h"
#include "mozIStorageValueArray.h"
#include "StorageBaseStatementInternal.h"
#include "mozilla/Attributes.h"

class nsIXPConnectJSObjectHolder;
struct sqlite3_stmt;

namespace mozilla {
namespace storage {
class StatementJSHelper;
class Connection;

class Statement final : public mozIStorageStatement
                      , public mozIStorageValueArray
                      , public StorageBaseStatementInternal
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENT
  NS_DECL_MOZISTORAGEBASESTATEMENT
  NS_DECL_MOZISTORAGEBINDINGPARAMS
  // NS_DECL_MOZISTORAGEVALUEARRAY (methods in mozIStorageStatement)
  NS_DECL_STORAGEBASESTATEMENTINTERNAL

  Statement();

  /**
   * Initializes the object on aDBConnection by preparing the SQL statement
   * given by aSQLStatement.
   *
   * @param aDBConnection
   *        The Connection object this statement is associated with.
   * @param aNativeConnection
   *        The native Sqlite connection this statement is associated with.
   * @param aSQLStatement
   *        The SQL statement to prepare that this object will represent.
   */
  nsresult initialize(Connection *aDBConnection,
                      sqlite3* aNativeConnection,
                      const nsACString &aSQLStatement);


  /**
   * Obtains the native statement pointer.
   */
  inline sqlite3_stmt *nativeStatement() { return mDBStatement; }

  /**
   * Obtains and transfers ownership of the array of parameters that are bound
   * to this statment.  This can be null.
   */
  inline already_AddRefed<BindingParamsArray> bindingParamsArray()
  {
    return mParamsArray.forget();
  }

private:
    ~Statement();

    sqlite3_stmt *mDBStatement;
    uint32_t mParamCount;
    uint32_t mResultColumnCount;
    nsTArray<nsCString> mColumnNames;
    bool mExecuting;

    /**
     * @return a pointer to the BindingParams object to use with our Bind*
     *         method.
     */
    mozIStorageBindingParams *getParams();

    /**
     * Holds the array of parameters to bind to this statement when we execute
     * it asynchronously.
     */
    nsRefPtr<BindingParamsArray> mParamsArray;

    /**
     * The following two members are only used with the JS helper.  They cache
     * the row and params objects.
     */
    nsMainThreadPtrHandle<nsIXPConnectJSObjectHolder> mStatementParamsHolder;
    nsMainThreadPtrHandle<nsIXPConnectJSObjectHolder> mStatementRowHolder;

  /**
   * Internal version of finalize that allows us to tell it if it is being
   * called from the destructor so it can know not to dispatch events that
   * require a reference to us.
   *
   * @param aDestructing
   *        Is the destructor calling?
   */
  nsresult internalFinalize(bool aDestructing);

  friend class StatementJSHelper;
};

} // storage
} // mozilla

#endif // mozStorageStatement_h
