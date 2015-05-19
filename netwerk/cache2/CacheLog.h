/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Cache2Log__h__
#define Cache2Log__h__

#include "mozilla/Logging.h"

namespace mozilla {
namespace net {

extern PRLogModuleInfo* GetCache2Log();
#define LOG(x)  PR_LOG(GetCache2Log(), PR_LOG_DEBUG, x)
#define LOG_ENABLED() PR_LOG_TEST(GetCache2Log(), PR_LOG_DEBUG)

} // net
} // mozilla

#endif
