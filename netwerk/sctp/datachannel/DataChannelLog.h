/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DataChannelLog_h
#define DataChannelLog_h

#include "base/basictypes.h"
#include "mozilla/Logging.h"

namespace mozilla {
extern mozilla::LazyLogModule gDataChannelLog;
}

#define DC_ERROR(args) \
  MOZ_LOG(mozilla::gDataChannelLog, mozilla::LogLevel::Error, args)

#define DC_WARN(args) \
  MOZ_LOG(mozilla::gDataChannelLog, mozilla::LogLevel::Warning, args)

#define DC_INFO(args) \
  MOZ_LOG(mozilla::gDataChannelLog, mozilla::LogLevel::Info, args)

#define DC_DEBUG(args) \
  MOZ_LOG(mozilla::gDataChannelLog, mozilla::LogLevel::Debug, args)

#define DC_VERBOSE(args) \
  MOZ_LOG(mozilla::gDataChannelLog, mozilla::LogLevel::Verbose, args)

#endif
