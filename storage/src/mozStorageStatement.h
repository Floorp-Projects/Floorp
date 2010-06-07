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

class nsIXPConnectJSObjectHolder;
struct sqlite3_stmt;

namespace mozilla {
namespace storage {
class StatementJSHelper;
class Connection;

class Statement : public mozIStorageStatement
                , public mozIStorageValueArray
                , public StorageBaseStatementInternal
{
public:
  NS_DECL_ISUPPORTS
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
   * @param aSQLStatement
   *        The SQL statement to prepare that this object will represent.
   */
  nsresult initialize(Connection *aDBConnection,
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
    PRUint32 mParamCount;
    PRUint32 mResultColumnCount;
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
    nsCOMPtr<nsIXPConnectJSObjectHolder> mStatementParamsHolder;
    nsCOMPtr<nsIXPConnectJSObjectHolder> mStatementRowHolder;

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
