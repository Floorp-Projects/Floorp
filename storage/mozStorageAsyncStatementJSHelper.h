/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_mozStorageAsyncStatementJSHelper_h_
#define mozilla_storage_mozStorageAsyncStatementJSHelper_h_

#include "nsIXPCScriptable.h"
#include "nsIXPConnect.h"

namespace mozilla {
namespace storage {

class AsyncStatement;
class AsyncStatementParams;

/**
 * A modified version of StatementJSHelper that only exposes the async-specific
 * 'params' helper.  We do not expose 'row' or 'step' as they do not apply to
 * us.
 */
class AsyncStatementJSHelper : public nsIXPCScriptable {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE

 private:
  nsresult getParams(AsyncStatement*, JSContext*, JSObject*, JS::Value*);
};

/**
 * Wrapper used to clean up the references JS helpers hold to the statement.
 * For cycle-avoidance reasons they do not hold reference-counted references,
 * so it is important we do this.
 */
class AsyncStatementParamsHolder final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit AsyncStatementParamsHolder(AsyncStatementParams* aParams)
      : mParams(aParams) {}

  AsyncStatementParams* Get() const {
    MOZ_ASSERT(mParams);
    return mParams;
  }

 private:
  virtual ~AsyncStatementParamsHolder();

  RefPtr<AsyncStatementParams> mParams;
};

}  // namespace storage
}  // namespace mozilla

#endif  // mozilla_storage_mozStorageAsyncStatementJSHelper_h_
