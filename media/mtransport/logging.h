/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef logging_h__
#define logging_h__

#include <sstream>
#include "mozilla/Logging.h"

#define ML_ERROR            mozilla::LogLevel::Error
#define ML_WARNING          mozilla::LogLevel::Warning
#define ML_NOTICE           mozilla::LogLevel::Info
#define ML_INFO             mozilla::LogLevel::Debug
#define ML_DEBUG            mozilla::LogLevel::Verbose

#define MOZ_MTLOG_MODULE(n) \
  static PRLogModuleInfo* getLogModule() {      \
    static PRLogModuleInfo* log;                \
    if (!log)                                   \
      log = PR_NewLogModule(n);                 \
    return log;                                 \
  }

#define MOZ_MTLOG(level, b) \
  do {                                                                  \
    if (MOZ_LOG_TEST(getLogModule(), level)) {                           \
      std::stringstream str;                                            \
      str << b;                                                         \
      MOZ_LOG(getLogModule(), level, ("%s", str.str().c_str()));         \
    }                                                                   \
  } while(0)

#endif // logging_h__
