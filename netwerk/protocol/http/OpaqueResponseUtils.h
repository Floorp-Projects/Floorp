/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_OpaqueResponseUtils_h
#define mozilla_net_OpaqueResponseUtils_h

#include "nsIContentPolicy.h"
#include "nsIStreamListener.h"

#include "mozilla/Variant.h"
#include "mozilla/Logging.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIContentSniffer;
static mozilla::LazyLogModule gORBLog("ORB");

namespace mozilla::net {

class HttpBaseChannel;
class nsHttpResponseHead;

enum class OpaqueResponseBlockedReason : uint32_t {
  ALLOWED_SAFE_LISTED,
  BLOCKED_BLOCKLISTED_NEVER_SNIFFED,
  BLOCKED_206_AND_BLOCKLISTED,
  BLOCKED_NOSNIFF_AND_EITHER_BLOCKLISTED_OR_TEXTPLAIN,
  BLOCKED_SHOULD_SNIFF
};

OpaqueResponseBlockedReason GetOpaqueResponseBlockedReason(
    const nsHttpResponseHead& aResponseHead);

// Returns a tuple of (rangeStart, rangeEnd, rangeTotal) from the input range
// header string if succeed.
Result<std::tuple<int64_t, int64_t, int64_t>, nsresult>
ParseContentRangeHeaderString(const nsAutoCString& aRangeStr);

bool IsFirstPartialResponse(nsHttpResponseHead& aResponseHead);

void LogORBError(nsILoadInfo* aLoadInfo, nsIURI* aURI);

class OpaqueResponseBlocker final : public nsIStreamListener {
  enum class State { Sniffing, Allowed, Blocked };

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER;

  OpaqueResponseBlocker(nsIStreamListener* aNext, HttpBaseChannel* aChannel);

  void AllowResponse();
  void BlockResponse(HttpBaseChannel* aChannel, nsresult aReason);
 private:
  virtual ~OpaqueResponseBlocker() = default;

  void MaybeORBSniff(nsIRequest* aRequest);

  void ResolveAndSendPending(HttpBaseChannel* aChannel, State aState,
                             nsresult aStatus);

  nsCOMPtr<nsIStreamListener> mNext;

  State mState = State::Sniffing;
  nsresult mStatus = NS_OK;
  bool mCheckIsOpaqueResponseAllowedAfterSniff = true;
};

}  // namespace mozilla::net

#endif  // mozilla_net_OpaqueResponseUtils_h
