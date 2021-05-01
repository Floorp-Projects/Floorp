/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_OpaqueResponseUtils_h
#define mozilla_net_OpaqueResponseUtils_h

namespace mozilla {
namespace net {

class nsHttpResponseHead;

bool IsOpaqueSafeListedMIMEType(const nsACString& aContentType);

bool IsOpaqueBlockListedMIMEType(const nsACString& aContentType);

bool IsOpaqueBlockListedNeverSniffedMIMEType(const nsACString& aContentType);

// Returns a tuple of (rangeStart, rangeEnd, rangeTotal) from the input range
// header string if succeed.
Result<std::tuple<int64_t, int64_t, int64_t>, nsresult>
ParseContentRangeHeaderString(const nsAutoCString& aRangeStr);

bool IsFirstPartialResponse(nsHttpResponseHead& aResponseHead);

class OpaqueResponseBlockingInfo final {
  const TimeStamp mStartTime;
  Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING mDestination;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OpaqueResponseBlockingInfo);

  explicit OpaqueResponseBlockingInfo(ExtContentPolicyType aContentPolicyType);

  void Report(const nsCString& aKey);

  void ReportContentLength(int64_t aContentLength);

 private:
  ~OpaqueResponseBlockingInfo() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_OpaqueResponseUtils_h
