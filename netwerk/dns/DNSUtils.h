/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DNSUtils_h_
#define DNSUtils_h_

#include "nsError.h"

class nsIURI;
class nsIChannel;

namespace mozilla {
namespace net {

class NetworkConnectivityService;
class TRR;

class DNSUtils final {
 private:
  friend class NetworkConnectivityService;
  friend class ObliviousHttpService;
  friend class TRR;
  static nsresult CreateChannelHelper(nsIURI* aUri, nsIChannel** aResult);
};

}  // namespace net
}  // namespace mozilla

#endif  // DNSUtils_h_
