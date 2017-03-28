/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PStunAddrsParams_h
#define PStunAddrsParams_h

#include "nsTArray.h"

#ifdef MOZ_WEBRTC
#include "mtransport/nricestunaddr.h"
#endif

namespace mozilla {
namespace net {

// Need to define typedef in .h file--can't seem to in ipdl.h file?
#ifdef MOZ_WEBRTC
typedef nsTArray<NrIceStunAddr> NrIceStunAddrArray;
#else
// a "dummy" typedef for --disabled-webrtc builds when the definition
// for NrIceStunAddr is not available (otherwise we get complaints
// about missing definitions for contructor and destructor)
typedef nsTArray<int> NrIceStunAddrArray;
#endif

} // namespace net
} // namespace mozilla

#endif // PStunAddrsParams_h
