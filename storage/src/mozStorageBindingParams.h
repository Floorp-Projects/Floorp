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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#ifndef mozStorageBindingParams_h
#define mozStorageBindingParams_h

#include "nsCOMArray.h"
#include "nsIVariant.h"
#include "nsInterfaceHashtable.h"

#include "mozStorageBindingParamsArray.h"
#include "mozStorageStatement.h"
#include "mozStorageAsyncStatement.h"

#include "mozIStorageBindingParams.h"
#include "IStorageBindingParamsInternal.h"

namespace mozilla {
namespace storage {

class BindingParams : public mozIStorageBindingParams
                    , public IStorageBindingParamsInternal
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEBINDINGPARAMS
  NS_DECL_ISTORAGEBINDINGPARAMSINTERNAL

  /**
   * Locks the parameters and prevents further modification to it (such as
   * binding more elements to it).
   */
  void lock();

  /**
   * Unlocks the parameters and allows modification to it again.
   *
   * @param aOwningStatement
   *        The statement that owns us.  We cleared this when we were locked,
   *        and our invariant requires us to have this, so you need to tell us
   *        again.
   */
  void unlock(Statement *aOwningStatement);

  /**
   * @returns the pointer to the owning BindingParamsArray.  Used by a
   *          BindingParamsArray to verify that we belong to it when added.
   */
  const mozIStorageBindingParamsArray *getOwner() const;

  BindingParams(mozIStorageBindingParamsArray *aOwningArray,
                Statement *aOwningStatement);
  virtual ~BindingParams() {}

protected:
  BindingParams(mozIStorageBindingParamsArray *aOwningArray);
  nsCOMArray<nsIVariant> mParameters;
  bool mLocked;

private:

  /**
   * Track the BindingParamsArray that created us until we are added to it.
   * (Once we are added we are locked and no one needs to look up our owner.)
   * Ref-counted since there is no invariant that guarantees it stays alive
   * otherwise.  This keeps mOwningStatement alive for us too since the array
   * also holds a reference.
   */
  nsCOMPtr<mozIStorageBindingParamsArray> mOwningArray;
  /**
   * Used in the synchronous binding case to map parameter names to indices.
   * Not reference-counted because this is only non-null as long as mOwningArray
   * is non-null and mOwningArray also holds a statement reference.
   */
  Statement *mOwningStatement;
  PRUint32 mParamCount;
};

/**
 * Adds late resolution of named parameters so they don't get resolved until we
 * try and bind the parameters on the async thread.  We also stop checking
 * parameter indices for being too big since we just just don't know how many
 * there are.
 *
 * We support *either* binding by name or binding by index.  Trying to do both
 * results in only binding by name at sqlite3_stmt bind time.
 */
class AsyncBindingParams : public BindingParams
{
public:
  NS_SCRIPTABLE NS_IMETHOD BindByName(const nsACString & aName,
                                      nsIVariant *aValue);
  NS_SCRIPTABLE NS_IMETHOD BindByIndex(PRUint32 aIndex, nsIVariant *aValue);

  virtual already_AddRefed<mozIStorageError> bind(sqlite3_stmt * aStatement);

  AsyncBindingParams(mozIStorageBindingParamsArray *aOwningArray);
  virtual ~AsyncBindingParams() {}

private:
  nsInterfaceHashtable<nsCStringHashKey, nsIVariant> mNamedParameters;

  struct NamedParameterIterationClosureThunk
  {
    AsyncBindingParams *self;
    sqlite3_stmt *statement;
    nsCOMPtr<mozIStorageError> err;
  };

  static PLDHashOperator iterateOverNamedParameters(const nsACString &aName,
                                                    nsIVariant *aValue,
                                                    void *voidClosureThunk);
};

} // namespace storage
} // namespace mozilla

#endif // mozStorageBindingParams_h
