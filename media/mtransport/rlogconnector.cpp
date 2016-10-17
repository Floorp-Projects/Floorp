/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

#include <cstdarg>

#include "rlogconnector.h"

#include <deque>
#include <string>
#include "logging.h"
#include "mozilla/Assertions.h"
#include "mozilla/Move.h" // Pinch hitting for <utility> and std::move
#include "mozilla/Mutex.h"
#include "mozilla/Sprintf.h"
#include <vector>

extern "C" {
#include <csi_platform.h>
#include "r_log.h"
#include "registry.h"
}

/* Matches r_dest_vlog type defined in r_log.h */
static int ringbuffer_vlog(int facility,
                           int level,
                           const char *format,
                           va_list ap) {
  MOZ_ASSERT(mozilla::RLogConnector::GetInstance());
  // I could be evil and printf right into a std::string, but unless this
  // shows up in profiling, it is not worth doing.
  char temp[4096];
  VsprintfLiteral(temp, format, ap);

  mozilla::RLogConnector::GetInstance()->Log(level, std::string(temp));
  return 0;
}

static mozilla::LogLevel rLogLvlToMozLogLvl(int level) {
  switch (level) {
    case LOG_EMERG:
    case LOG_ALERT:
    case LOG_CRIT:
    case LOG_ERR:
      return mozilla::LogLevel::Error;
    case LOG_WARNING:
      return mozilla::LogLevel::Warning;
    case LOG_NOTICE:
      return mozilla::LogLevel::Info;
    case LOG_INFO:
      return mozilla::LogLevel::Debug;
    case LOG_DEBUG:
    default:
      return mozilla::LogLevel::Verbose;
  }
}

MOZ_MTLOG_MODULE("nicer");

namespace mozilla {

RLogConnector* RLogConnector::instance;

RLogConnector::RLogConnector()
  : log_limit_(4096),
    mutex_("RLogConnector::mutex_"),
    disableCount_(0) {
}

RLogConnector::~RLogConnector() {
}

void RLogConnector::SetLogLimit(uint32_t new_limit) {
  OffTheBooksMutexAutoLock lock(mutex_);
  log_limit_ = new_limit;
  RemoveOld();
}

void RLogConnector::Log(int level, std::string&& log) {
  MOZ_MTLOG(rLogLvlToMozLogLvl(level), log);

  if (level <= LOG_INFO) {
    OffTheBooksMutexAutoLock lock(mutex_);
    if (disableCount_ == 0) {
      AddMsg(Move(log));
    }
  }
}

void RLogConnector::AddMsg(std::string&& msg) {
  log_messages_.push_front(Move(msg));
  RemoveOld();
}

inline void RLogConnector::RemoveOld() {
  if (log_messages_.size() > log_limit_) {
    log_messages_.resize(log_limit_);
  }
}

RLogConnector* RLogConnector::CreateInstance() {
  if (!instance) {
    instance = new RLogConnector;
    NR_reg_init(NR_REG_MODE_LOCAL);
    r_log_set_extra_destination(LOG_DEBUG, &ringbuffer_vlog);
  }
  return instance;
}

RLogConnector* RLogConnector::GetInstance() {
  return instance;
}

void RLogConnector::DestroyInstance() {
  // First param is ignored when passing null
  r_log_set_extra_destination(LOG_DEBUG, nullptr);
  delete instance;
  instance = nullptr;
}

// As long as at least one PeerConnection exists in a Private Window rlog messages will not
// be saved in the RLogConnector. This is necessary because the log_messages buffer
// is shared across all instances of PeerConnectionImpls. There is no way with the current
// structure of r_log to run separate logs.

void RLogConnector::EnterPrivateMode() {
  OffTheBooksMutexAutoLock lock(mutex_);
  ++disableCount_;
  MOZ_ASSERT(disableCount_ != 0);

  if (disableCount_ == 1) {
    AddMsg("LOGGING SUSPENDED: a connection is active in a Private Window ***");
  }
}

void RLogConnector::ExitPrivateMode() {
  OffTheBooksMutexAutoLock lock(mutex_);
  MOZ_ASSERT(disableCount_ != 0);

  if (--disableCount_ == 0) {
    AddMsg("LOGGING RESUMED: no connections are active in a Private Window ***");
  }
}

void RLogConnector::Clear() {
  OffTheBooksMutexAutoLock lock(mutex_);
  log_messages_.clear();
}

void RLogConnector::Filter(const std::string& substring,
                            uint32_t limit,
                            std::deque<std::string>* matching_logs) {
  std::vector<std::string> substrings;
  substrings.push_back(substring);
  FilterAny(substrings, limit, matching_logs);
}

inline bool AnySubstringMatches(const std::vector<std::string>& substrings,
                                const std::string& string) {
  for (auto sub = substrings.begin(); sub != substrings.end(); ++sub) {
    if (string.find(*sub) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void RLogConnector::FilterAny(const std::vector<std::string>& substrings,
                               uint32_t limit,
                               std::deque<std::string>* matching_logs) {
  OffTheBooksMutexAutoLock lock(mutex_);
  if (limit == 0) {
    // At a max, all of the log messages.
    limit = log_limit_;
  }

  for (auto log = log_messages_.begin();
       log != log_messages_.end() && matching_logs->size() < limit;
       ++log) {
    if (AnySubstringMatches(substrings, *log)) {
      matching_logs->push_front(*log);
    }
  }
}

} // namespace mozilla
