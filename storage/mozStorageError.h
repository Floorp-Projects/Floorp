/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageError_h
#define mozStorageError_h

#include "mozIStorageError.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace storage {

class Error final : public mozIStorageError {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEERROR

  Error(int aResult, const char* aMessage);

 private:
  ~Error() {}

  int mResult;
  nsCString mMessage;
};

}  // namespace storage
}  // namespace mozilla

#endif  // mozStorageError_h
