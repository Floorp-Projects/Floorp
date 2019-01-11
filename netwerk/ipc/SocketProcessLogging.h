/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SocketProcessLogging_h
#define mozilla_SocketProcessLogging_h

#include "mozilla/Logging.h"

namespace mozilla {
namespace net {
extern LazyLogModule gSocketProcessLog;
}
}  // namespace mozilla

#define LOG(msg) MOZ_LOG(gSocketProcessLog, mozilla::LogLevel::Debug, msg)
#define LOG_ENABLED() MOZ_LOG_TEST(gSocketProcessLog, mozilla::LogLevel::Debug)

#endif  // mozilla_SocketProcessLogging_h
