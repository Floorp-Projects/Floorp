/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PStunAddrsParams_h
#define PStunAddrsParams_h

#include "mtransport/nricestunaddr.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

// Need to define typedef in .h file--can't seem to in ipdl.h file?
typedef nsTArray<NrIceStunAddr> NrIceStunAddrArray;

} // namespace net
} // namespace mozilla

#endif // PStunAddrsParams_h
