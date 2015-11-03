/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Cache2Log__h__
#define Cache2Log__h__

#include "mozilla/Logging.h"

namespace mozilla {
namespace net {

extern LazyLogModule gCache2Log;
#define LOG(x)  MOZ_LOG(gCache2Log, mozilla::LogLevel::Debug, x)
#define LOG_ENABLED() MOZ_LOG_TEST(gCache2Log, mozilla::LogLevel::Debug)

} // namespace net
} // namespace mozilla

#endif
