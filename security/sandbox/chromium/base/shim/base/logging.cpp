/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/logging.h"
#ifdef OS_WIN
#include <windows.h>
#endif

namespace {
int min_log_level = 0;
}

namespace logging
{

DcheckState g_dcheck_state = DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;

DcheckState get_dcheck_state() {
  return g_dcheck_state;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       int ctr) :
  line_(line)
{
}

LogMessage::LogMessage(const char* file, int line, int ctr) : line_(line)
{
}

LogMessage::LogMessage(const char* file, int line, std::string* result) :
  severity_(LOG_FATAL),
  file_(file),
  line_(line)
{
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       std::string* result) :
  severity_(severity),
  file_(file),
  line_(line)
{
}

LogMessage::~LogMessage()
{
}

int GetMinLogLevel()
{
  return min_log_level;
}

int GetVlogLevelHelper(const char* file, size_t N)
{
  return 0;
}

void RawLog(int level, const char* message)
{
}

#ifdef OS_WIN
LogMessage::SaveLastError::SaveLastError() :
  last_error_(::GetLastError())
{
}

LogMessage::SaveLastError::~SaveLastError()
{
  ::SetLastError(last_error_);
}

SystemErrorCode GetLastSystemErrorCode()
{
  return ::GetLastError();
}

Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file, int line,
                                           LogSeverity severity,
                                           SystemErrorCode err,
                                           const char* module) :
  err_(err),
  module_(module),
  log_message_(file, line, severity)
{
}

Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err) :
  err_(err),
  module_(NULL),
  log_message_(file, line, severity)
{
}

Win32ErrorLogMessage::~Win32ErrorLogMessage()
{
}
#endif // OS_WIN

} // namespace logging
