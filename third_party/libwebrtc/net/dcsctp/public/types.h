/*
 *  Copyright 2019 The Chromium Authors. All rights reserved.
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PUBLIC_TYPES_H_
#define NET_DCSCTP_PUBLIC_TYPES_H_

#include "net/dcsctp/public/strong_alias.h"

namespace dcsctp {

// Stream Identifier
using StreamID = StrongAlias<class StreamIDTag, uint16_t>;

// Payload Protocol Identifier (PPID)
using PPID = StrongAlias<class PPIDTag, uint16_t>;

// Timeout Identifier
using TimeoutID = StrongAlias<class TimeoutTag, uint64_t>;

// Indicates if a message is allowed to be received out-of-order compared to
// other messages on the same stream.
using IsUnordered = StrongAlias<class IsUnorderedTag, bool>;

// Duration, as milliseconds. Overflows after 24 days.
using DurationMs = StrongAlias<class DurationMsTag, int32_t>;

// Current time, in milliseconds since a client-defined epoch.´
using TimeMs = StrongAlias<class TimeMsTag, int64_t>;

}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_TYPES_H_
