/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/timer/timer.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "net/dcsctp/public/timeout.h"

namespace dcsctp {
namespace {
TimeoutID MakeTimeoutId(uint32_t timer_id, uint32_t generation) {
  return TimeoutID(static_cast<uint64_t>(timer_id) << 32 | generation);
}

DurationMs GetBackoffDuration(TimerBackoffAlgorithm algorithm,
                              DurationMs base_duration,
                              int expiration_count) {
  switch (algorithm) {
    case TimerBackoffAlgorithm::kFixed:
      return base_duration;
    case TimerBackoffAlgorithm::kExponential:
      return DurationMs(*base_duration * (1 << expiration_count));
  }
}
}  // namespace

Timer::Timer(uint32_t id,
             absl::string_view name,
             OnExpired on_expired,
             UnregisterHandler unregister_handler,
             std::unique_ptr<Timeout> timeout,
             const TimerOptions& options)
    : id_(id),
      name_(name),
      options_(options),
      on_expired_(std::move(on_expired)),
      unregister_handler_(std::move(unregister_handler)),
      timeout_(std::move(timeout)),
      duration_(options.duration) {}

Timer::~Timer() {
  Stop();
  unregister_handler_();
}

void Timer::Start() {
  expiration_count_ = 0;
  if (!is_running()) {
    is_running_ = true;
    timeout_->Start(duration_, MakeTimeoutId(id_, ++generation_));
  } else {
    // Timer was running - stop and restart it, to make it expire in `duration_`
    // from now.
    timeout_->Restart(duration_, MakeTimeoutId(id_, ++generation_));
  }
}

void Timer::Stop() {
  if (is_running()) {
    timeout_->Stop();
    expiration_count_ = 0;
    is_running_ = false;
  }
}

void Timer::Trigger(uint32_t generation) {
  if (is_running_ && generation == generation_) {
    ++expiration_count_;
    if (options_.max_restarts >= 0 &&
        expiration_count_ > options_.max_restarts) {
      is_running_ = false;
    }

    absl::optional<DurationMs> new_duration = on_expired_();
    if (new_duration.has_value()) {
      duration_ = new_duration.value();
    }

    if (is_running_) {
      // Restart it with new duration.
      DurationMs duration = GetBackoffDuration(options_.backoff_algorithm,
                                               duration_, expiration_count_);
      timeout_->Start(duration, MakeTimeoutId(id_, ++generation_));
    }
  }
}

void TimerManager::HandleTimeout(TimeoutID timeout_id) {
  uint32_t timer_id = *timeout_id >> 32;
  uint32_t generation = *timeout_id;
  auto it = timers_.find(timer_id);
  if (it != timers_.end()) {
    it->second->Trigger(generation);
  }
}

std::unique_ptr<Timer> TimerManager::CreateTimer(absl::string_view name,
                                                 Timer::OnExpired on_expired,
                                                 const TimerOptions& options) {
  uint32_t id = ++next_id_;
  auto timer = absl::WrapUnique(new Timer(
      id, name, std::move(on_expired), [this, id]() { timers_.erase(id); },
      create_timeout_(), options));
  timers_[id] = timer.get();
  return timer;
}

}  // namespace dcsctp
