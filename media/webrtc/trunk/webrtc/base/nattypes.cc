/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/base/nattypes.h"

namespace rtc {

class SymmetricNAT : public NAT {
public:
  bool IsSymmetric() { return true; }
  bool FiltersIP() { return true; }
  bool FiltersPort() { return true; }
};

class OpenConeNAT : public NAT {
public:
  bool IsSymmetric() { return false; }
  bool FiltersIP() { return false; }
  bool FiltersPort() { return false; }
};

class AddressRestrictedNAT : public NAT {
public:
  bool IsSymmetric() { return false; }
  bool FiltersIP() { return true; }
  bool FiltersPort() { return false; }
};

class PortRestrictedNAT : public NAT {
public:
  bool IsSymmetric() { return false; }
  bool FiltersIP() { return true; }
  bool FiltersPort() { return true; }
};

NAT* NAT::Create(NATType type) {
  switch (type) {
  case NAT_OPEN_CONE:       return new OpenConeNAT();
  case NAT_ADDR_RESTRICTED: return new AddressRestrictedNAT();
  case NAT_PORT_RESTRICTED: return new PortRestrictedNAT();
  case NAT_SYMMETRIC:       return new SymmetricNAT();
  default: assert(0);       return 0;
  }
}

} // namespace rtc
