/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"

namespace mozilla {
namespace net {

// Log module for cache2 (2013) cache implementation logging...
//
// To enable logging (see prlog.h for full details):
//
//    set MOZ_LOG=cache2:5
//    set MOZ_LOG_FILE=network.log
//
// This enables LogLevel::Debug level information and places all output in
// the file network.log.
LazyLogModule gCache2Log("cache2");

} // namespace net
} // namespace mozilla
