/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_mozStorageAsyncStatementParams_h_
#define mozilla_storage_mozStorageAsyncStatementParams_h_

#include "mozIStorageStatementParams.h"
#include "nsIXPCScriptable.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace storage {

class AsyncStatement;

/*
 * Since mozIStorageStatementParams is just a tagging interface we do not have
 * an async variant.
 */
class AsyncStatementParams final : public mozIStorageStatementParams
                                 , public nsIXPCScriptable
{
public:
  explicit AsyncStatementParams(AsyncStatement *aStatement);

  // interfaces
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTPARAMS
  NS_DECL_NSIXPCSCRIPTABLE

protected:
  virtual ~AsyncStatementParams() {}

  AsyncStatement *mStatement;

  friend class AsyncStatementParamsHolder;
};

} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_mozStorageAsyncStatementParams_h_
