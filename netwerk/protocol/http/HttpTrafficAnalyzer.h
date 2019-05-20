/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_protocol_http_HttpTrafficAnalyzer_h
#define mozilla_netwerk_protocol_http_HttpTrafficAnalyzer_h

namespace mozilla {
namespace net {

#define DEFINE_CATEGORY(_name, _idx) e##_name = _idx##u,
enum HttpTrafficCategory : uint8_t {
#include "HttpTrafficAnalyzer.inc"
  eInvalid = 255,
};
#undef DEFINE_CATEGORY

class HttpTrafficAnalyzer final {
 public:
  enum ClassOfService : uint8_t {
    eLeader = 0,
    eBackground = 1,
    eOther = 255,
  };

  enum TrackingClassification : uint8_t {
    eNone = 0,
    eBasic = 1,
    eContent = 2,
    eFingerprinting = 3,
  };

  static HttpTrafficCategory CreateTrafficCategory(
      bool aIsPrivateMode, bool aIsSystemPrincipal, bool aIsThirdParty,
      ClassOfService aClassOfService, TrackingClassification aClassification);

  nsresult IncrementHttpTransaction(HttpTrafficCategory aCategory);
  nsresult IncrementHttpConnection(HttpTrafficCategory aCategory);
  nsresult IncrementHttpConnection(nsTArray<HttpTrafficCategory>&& aCategories);
  nsresult AccumulateHttpTransferredSize(HttpTrafficCategory aCategory,
                                         uint64_t aBytesRead,
                                         uint64_t aBytesSent);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_netwerk_protocol_http_HttpTrafficAnalyzer_h
