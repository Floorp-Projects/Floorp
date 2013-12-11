/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef logging_h__
#define logging_h__

#if defined(PR_LOG)
#error "Must #include logging.h before any IPDL-generated files or other files that #include prlog.h."
#endif

// Enforce logging under production builds for MOZ_MTLOG friends.
#ifndef PR_LOGGING
#define FORCE_PR_LOG 1
#endif

#include <sstream>
#include <prlog.h>

#if defined(PR_LOGGING)

#define ML_EMERG            1
#define ML_ERROR            2
#define ML_WARNING          3
#define ML_NOTICE           4
#define ML_INFO             5
#define ML_DEBUG            6

// PR_LOGGING is on --> make useful MTLOG macros
#define MOZ_MTLOG_MODULE(n) \
  static PRLogModuleInfo* getLogModule() {      \
    static PRLogModuleInfo* log;                \
    if (!log)                                   \
      log = PR_NewLogModule(n);                 \
    return log;                                 \
  }

#define MOZ_MTLOG(level, b) \
  do {                                                                  \
    std::stringstream str;                                              \
    str << b;                                                           \
    PR_LOG(getLogModule(), level, ("%s", str.str().c_str())); } while(0)

#else
// PR_LOGGING is off --> make no-op MTLOG macros
#define MOZ_MTLOG_MODULE(n)
#define MOZ_MTLOG(level, b)

#endif // defined(PR_LOGGING)

#endif // logging_h__
