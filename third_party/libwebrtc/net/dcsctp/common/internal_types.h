/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_COMMON_INTERNAL_TYPES_H_
#define NET_DCSCTP_COMMON_INTERNAL_TYPES_H_

#include <utility>

#include "net/dcsctp/public/strong_alias.h"

namespace dcsctp {

// Stream Sequence Number (SSN)
using SSN = StrongAlias<class SSNTag, uint16_t>;

// Message Identifier (MID)
using MID = StrongAlias<class MIDTag, uint32_t>;

// Fragment Sequence Number (FSN)
using FSN = StrongAlias<class FSNTag, uint32_t>;

// Transmission Sequence Number (TSN)
using TSN = StrongAlias<class TSNTag, uint32_t>;

// Reconfiguration Request Sequence Number
using ReconfigRequestSN = StrongAlias<class ReconfigRequestSNTag, uint32_t>;

// Reconfiguration Response Sequence Number
using ReconfigResponseSN = StrongAlias<class ReconfigResponseSNTag, uint32_t>;

// Verification Tag, used for packet validation.
using VerificationTag = StrongAlias<class VerificationTagTag, uint32_t>;

}  // namespace dcsctp
#endif  // NET_DCSCTP_COMMON_INTERNAL_TYPES_H_
