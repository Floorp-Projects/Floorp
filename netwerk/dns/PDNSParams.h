/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=c: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDNSParams_h
#define PDNSParams_h

#include "DNS.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

// Need to define typedef in .h file--can't seem to in ipdl.h file?
typedef nsTArray<NetAddr> NetAddrArray;

} // namespace net
} // namespace mozilla

#endif // PDNSParams_h
