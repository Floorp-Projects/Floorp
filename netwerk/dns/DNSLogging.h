/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DNSLogging_h
#define mozilla_DNSLogging_h

#include "mozilla/Logging.h"

#undef LOG

namespace mozilla {
namespace net {
extern LazyLogModule gHostResolverLog;
}  // namespace net
}  // namespace mozilla

#define LOG(msg) \
  MOZ_LOG(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug, msg)
#define LOG1(msg) \
  MOZ_LOG(mozilla::net::gHostResolverLog, mozilla::LogLevel::Error, msg)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)

#endif  // mozilla_DNSLogging_h
