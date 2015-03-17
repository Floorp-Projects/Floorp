/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageBindingParams_h
#define mozStorageBindingParams_h

#include "nsCOMArray.h"
#include "nsIVariant.h"
#include "nsInterfaceHashtable.h"

#include "mozStorageBindingParamsArray.h"
#include "mozStorageStatement.h"
#include "mozStorageAsyncStatement.h"
#include "Variant.h"

#include "mozIStorageBindingParams.h"
#include "IStorageBindingParamsInternal.h"

namespace mozilla {
namespace storage {

class BindingParams : public mozIStorageBindingParams
                    , public IStorageBindingParamsInternal
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
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
protected:
  virtual ~BindingParams() {}

  explicit BindingParams(mozIStorageBindingParamsArray *aOwningArray);
  // Note that this is managed as a sparse array, so particular caution should
  // be used for out-of-bounds usage.
  nsTArray<nsRefPtr<Variant_base> > mParameters;
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
  uint32_t mParamCount;
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
  NS_IMETHOD BindByName(const nsACString & aName,
                                      nsIVariant *aValue);
  NS_IMETHOD BindByIndex(uint32_t aIndex, nsIVariant *aValue);

  virtual already_AddRefed<mozIStorageError> bind(sqlite3_stmt * aStatement);

  explicit AsyncBindingParams(mozIStorageBindingParamsArray *aOwningArray);
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
