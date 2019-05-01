/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozStorageBindingParamsArray.h"
#include "mozStorageBindingParams.h"
#include "StorageBaseStatementInternal.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// BindingParamsArray

BindingParamsArray::BindingParamsArray(
    StorageBaseStatementInternal* aOwningStatement)
    : mOwningStatement(aOwningStatement), mLocked(false) {}

void BindingParamsArray::lock() {
  NS_ASSERTION(mLocked == false, "Array has already been locked!");
  mLocked = true;

  // We also no longer need to hold a reference to our statement since it owns
  // us.
  mOwningStatement = nullptr;
}

const StorageBaseStatementInternal* BindingParamsArray::getOwner() const {
  return mOwningStatement;
}

NS_IMPL_ISUPPORTS(BindingParamsArray, mozIStorageBindingParamsArray)

///////////////////////////////////////////////////////////////////////////////
//// mozIStorageBindingParamsArray

NS_IMETHODIMP
BindingParamsArray::NewBindingParams(mozIStorageBindingParams** _params) {
  NS_ENSURE_FALSE(mLocked, NS_ERROR_UNEXPECTED);

  nsCOMPtr<mozIStorageBindingParams> params(
      mOwningStatement->newBindingParams(this));
  NS_ENSURE_TRUE(params, NS_ERROR_UNEXPECTED);

  params.forget(_params);
  return NS_OK;
}

NS_IMETHODIMP
BindingParamsArray::AddParams(mozIStorageBindingParams* aParameters) {
  NS_ENSURE_FALSE(mLocked, NS_ERROR_UNEXPECTED);

  BindingParams* params = static_cast<BindingParams*>(aParameters);

  // Check to make sure that this set of parameters was created with us.
  if (params->getOwner() != this) return NS_ERROR_UNEXPECTED;

  NS_ENSURE_TRUE(mArray.AppendElement(params), NS_ERROR_OUT_OF_MEMORY);

  // Lock the parameters only after we've successfully added them.
  params->lock();

  return NS_OK;
}

NS_IMETHODIMP
BindingParamsArray::GetLength(uint32_t* _length) {
  *_length = length();
  return NS_OK;
}

}  // namespace storage
}  // namespace mozilla
