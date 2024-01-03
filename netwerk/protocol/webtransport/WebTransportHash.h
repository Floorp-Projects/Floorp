/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportHash_h
#define mozilla_net_WebTransportHash_h

#include "nsIWebTransport.h"

namespace mozilla::net {

class WebTransportHash final : public nsIWebTransportHash {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBTRANSPORTHASH

  explicit WebTransportHash(const nsACString& algorithm,
                            const nsTArray<uint8_t>& value)
      : mAlgorithm(algorithm), mValue(Span(value)) {}

 private:
  ~WebTransportHash() = default;
  nsCString mAlgorithm;
  nsTArray<uint8_t> mValue;
};

}  // namespace mozilla::net

#endif  // mozilla_net_WebTransportHash_h
