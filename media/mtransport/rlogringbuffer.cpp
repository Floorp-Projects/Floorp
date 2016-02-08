/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

#include <cstdarg>

#include "rlogringbuffer.h"

#include <deque>
#include <string>
#include "mozilla/Assertions.h"
#include "mozilla/Move.h" // Pinch hitting for <utility> and std::move
#include "mozilla/Mutex.h"
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
  MOZ_ASSERT(mozilla::RLogRingBuffer::GetInstance());
  // I could be evil and printf right into a std::string, but unless this
  // shows up in profiling, it is not worth doing.
  char temp[4096];
  vsnprintf(temp, sizeof(temp), format, ap);

  mozilla::RLogRingBuffer::GetInstance()->Log(std::string(temp));
  return 0;
}

namespace mozilla {

RLogRingBuffer* RLogRingBuffer::instance;

RLogRingBuffer::RLogRingBuffer()
  : log_limit_(4096),
    mutex_("RLogRingBuffer::mutex_"),
    disableCount_(0) {
}

RLogRingBuffer::~RLogRingBuffer() {
}

void RLogRingBuffer::SetLogLimit(uint32_t new_limit) {
  OffTheBooksMutexAutoLock lock(mutex_);
  log_limit_ = new_limit;
  RemoveOld();
}

void RLogRingBuffer::Log(std::string&& log) {
  OffTheBooksMutexAutoLock lock(mutex_);
  if (disableCount_ == 0) {
    AddMsg(Move(log));
  }
}

void RLogRingBuffer::AddMsg(std::string&& msg) {
  log_messages_.push_front(Move(msg));
  RemoveOld();
}

inline void RLogRingBuffer::RemoveOld() {
  if (log_messages_.size() > log_limit_) {
    log_messages_.resize(log_limit_);
  }
}

RLogRingBuffer* RLogRingBuffer::CreateInstance() {
  if (!instance) {
    instance = new RLogRingBuffer;
    NR_reg_init(NR_REG_MODE_LOCAL);
    r_log_set_extra_destination(LOG_INFO, &ringbuffer_vlog);
  }
  return instance;
}

RLogRingBuffer* RLogRingBuffer::GetInstance() {
  return instance;
}

void RLogRingBuffer::DestroyInstance() {
  // First param is ignored when passing null
  r_log_set_extra_destination(LOG_INFO, nullptr);
  delete instance;
  instance = nullptr;
}

// As long as at least one PeerConnection exists in a Private Window rlog messages will not
// be saved in the RLogRingBuffer. This is necessary because the log_messages buffer
// is shared across all instances of PeerConnectionImpls. There is no way with the current
// structure of r_log to run separate logs.

void RLogRingBuffer::EnterPrivateMode() {
  OffTheBooksMutexAutoLock lock(mutex_);
  ++disableCount_;
  MOZ_ASSERT(disableCount_ != 0);

  if (disableCount_ == 1) {
    AddMsg("LOGGING SUSPENDED: a connection is active in a Private Window ***");
  }
}

void RLogRingBuffer::ExitPrivateMode() {
  OffTheBooksMutexAutoLock lock(mutex_);
  MOZ_ASSERT(disableCount_ != 0);

  if (--disableCount_ == 0) {
    AddMsg("LOGGING RESUMED: no connections are active in a Private Window ***");
  }
}

void RLogRingBuffer::Clear() {
  OffTheBooksMutexAutoLock lock(mutex_);
  log_messages_.clear();
}

void RLogRingBuffer::Filter(const std::string& substring,
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

void RLogRingBuffer::FilterAny(const std::vector<std::string>& substrings,
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
