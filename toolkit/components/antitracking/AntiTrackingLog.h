/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackinglog_h
#define mozilla_antitrackinglog_h

#include "mozilla/Logging.h"
#include "nsString.h"

namespace mozilla {

extern LazyLogModule gAntiTrackingLog;
static const nsCString::size_type sMaxSpecLength = 128;

#define LOG(format) MOZ_LOG(gAntiTrackingLog, mozilla::LogLevel::Debug, format)

#define LOG_SPEC(format, uri)                                       \
  PR_BEGIN_MACRO                                                    \
  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {   \
    nsAutoCString _specStr("(null)"_ns);                            \
    if (uri) {                                                      \
      _specStr = (uri)->GetSpecOrDefault();                         \
    }                                                               \
    _specStr.Truncate(std::min(_specStr.Length(), sMaxSpecLength)); \
    const char* _spec = _specStr.get();                             \
    LOG(format);                                                    \
  }                                                                 \
  PR_END_MACRO

#define LOG_SPEC2(format, uri1, uri2)                                 \
  PR_BEGIN_MACRO                                                      \
  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {     \
    nsAutoCString _specStr1("(null)"_ns);                             \
    if (uri1) {                                                       \
      _specStr1 = (uri1)->GetSpecOrDefault();                         \
    }                                                                 \
    _specStr1.Truncate(std::min(_specStr1.Length(), sMaxSpecLength)); \
    const char* _spec1 = _specStr1.get();                             \
    nsAutoCString _specStr2("(null)"_ns);                             \
    if (uri2) {                                                       \
      _specStr2 = (uri2)->GetSpecOrDefault();                         \
    }                                                                 \
    _specStr2.Truncate(std::min(_specStr2.Length(), sMaxSpecLength)); \
    const char* _spec2 = _specStr2.get();                             \
    LOG(format);                                                      \
  }                                                                   \
  PR_END_MACRO

#define LOG_PRIN(format, principal)                                 \
  PR_BEGIN_MACRO                                                    \
  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {   \
    nsAutoCString _specStr("(null)"_ns);                            \
    if (principal) {                                                \
      (principal)->GetAsciiSpec(_specStr);                          \
    }                                                               \
    _specStr.Truncate(std::min(_specStr.Length(), sMaxSpecLength)); \
    const char* _spec = _specStr.get();                             \
    LOG(format);                                                    \
  }                                                                 \
  PR_END_MACRO
}  // namespace mozilla

#endif  // mozilla_antitrackinglog_h
