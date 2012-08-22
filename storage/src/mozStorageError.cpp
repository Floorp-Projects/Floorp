/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozStorageError.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Error

Error::Error(int aResult,
             const char *aMessage)
: mResult(aResult)
, mMessage(aMessage)
{
}

/**
 * Note:  This object is only ever accessed on one thread at a time.  It it not
 *        threadsafe, but it does need threadsafe AddRef and Release.
 */
NS_IMPL_THREADSAFE_ISUPPORTS1(
  Error,
  mozIStorageError
)

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageError

NS_IMETHODIMP
Error::GetResult(int32_t *_result)
{
  *_result = mResult;
  return NS_OK;
}

NS_IMETHODIMP
Error::GetMessage(nsACString &_message)
{
  _message = mMessage;
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
