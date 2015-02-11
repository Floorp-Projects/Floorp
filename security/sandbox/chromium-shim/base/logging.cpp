/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a stripped down version of the Chromium source file base/logging.cc
// This prevents dependency on the Chromium logging and dependency creep in
// general.
// At some point we should find a way to hook this into our own logging see
// bug 1013988.
// The formatting in this file matches the original Chromium file to aid future
// merging.

#include "base/logging.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_POSIX)
#include <errno.h>
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace logging {

namespace {

int min_log_level = 0;

}  // namespace

int GetMinLogLevel() {
  return min_log_level;
}

int GetVlogLevelHelper(const char* file, size_t N) {
  return 0;
}

// Explicit instantiations for commonly used comparisons.
template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
template std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);

#if defined(OS_WIN)
LogMessage::SaveLastError::SaveLastError() : last_error_(::GetLastError()) {
}

LogMessage::SaveLastError::~SaveLastError() {
  ::SetLastError(last_error_);
}
#endif  // defined(OS_WIN)

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
}

LogMessage::LogMessage(const char* file, int line, std::string* result)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  delete result;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line) {
  delete result;
}

LogMessage::~LogMessage() {
}

SystemErrorCode GetLastSystemErrorCode() {
#if defined(OS_WIN)
  return ::GetLastError();
#elif defined(OS_POSIX)
  return errno;
#else
#error Not implemented
#endif
}

#if defined(OS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
}
#elif defined(OS_POSIX)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

ErrnoLogMessage::~ErrnoLogMessage() {
}
#endif  // OS_WIN

void RawLog(int level, const char* message) {
}

} // namespace logging

#if defined(OS_WIN)
std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
  return out << base::WideToUTF8(std::wstring(wstr));
}
#endif
