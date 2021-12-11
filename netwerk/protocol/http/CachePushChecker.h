/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CachePushChecker_h__
#define CachePushChecker_h__

#include <functional>
#include "mozilla/OriginAttributes.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsIEventTarget.h"
#include "nsString.h"
#include "nsIURI.h"

namespace mozilla {
namespace net {

class CachePushChecker final : public nsICacheEntryOpenCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHEENTRYOPENCALLBACK

  explicit CachePushChecker(nsIURI* aPushedURL,
                            const OriginAttributes& aOriginAttributes,
                            const nsACString& aRequestString,
                            std::function<void(bool)>&& aCallback);
  nsresult DoCheck();

 private:
  ~CachePushChecker() = default;
  void InvokeCallback(bool aResult);

  nsCOMPtr<nsIURI> mPushedURL;
  OriginAttributes mOriginAttributes;
  nsCString mRequestString;
  std::function<void(bool)> mCallback;
  nsCOMPtr<nsIEventTarget> mCurrentEventTarget;
};

}  // namespace net
}  // namespace mozilla

#endif
