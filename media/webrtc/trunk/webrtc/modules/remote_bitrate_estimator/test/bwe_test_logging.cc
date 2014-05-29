/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"

#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {
namespace testing {
namespace bwe {

Logging Logging::g_Logging;

static std::string ToString(uint32_t v) {
  const size_t kBufferSize = 16;
  char string_buffer[kBufferSize] = {0};
#if defined(_MSC_VER) && defined(_WIN32)
  _snprintf(string_buffer, kBufferSize - 1, "%08x", v);
#else
  snprintf(string_buffer, kBufferSize, "%08x", v);
#endif
  return string_buffer;
}

Logging::Context::Context(uint32_t name, int64_t timestamp_ms, bool enabled) {
  Logging::GetInstance()->PushState(ToString(name), timestamp_ms, enabled);
}

Logging::Context::Context(const std::string& name, int64_t timestamp_ms,
                          bool enabled) {
  Logging::GetInstance()->PushState(name, timestamp_ms, enabled);
}

Logging::Context::Context(const char* name, int64_t timestamp_ms,
                          bool enabled) {
  Logging::GetInstance()->PushState(name, timestamp_ms, enabled);
}

Logging::Context::~Context() {
  Logging::GetInstance()->PopState();
}

Logging* Logging::GetInstance() {
  return &g_Logging;
}

void Logging::SetGlobalContext(uint32_t name) {
  CriticalSectionScoped cs(crit_sect_.get());
  thread_map_[ThreadWrapper::GetThreadId()].global_state.tag = ToString(name);
}

void Logging::SetGlobalContext(const std::string& name) {
  CriticalSectionScoped cs(crit_sect_.get());
  thread_map_[ThreadWrapper::GetThreadId()].global_state.tag = name;
}

void Logging::SetGlobalContext(const char* name) {
  CriticalSectionScoped cs(crit_sect_.get());
  thread_map_[ThreadWrapper::GetThreadId()].global_state.tag = name;
}

void Logging::SetGlobalEnable(bool enabled) {
  CriticalSectionScoped cs(crit_sect_.get());
  thread_map_[ThreadWrapper::GetThreadId()].global_state.enabled = enabled;
}

void Logging::Log(const char format[], ...) {
  CriticalSectionScoped cs(crit_sect_.get());
  ThreadMap::iterator it = thread_map_.find(ThreadWrapper::GetThreadId());
  assert(it != thread_map_.end());
  const State& state = it->second.stack.top();
  if (state.enabled) {
    printf("%s\t", state.tag.c_str());
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
  }
}

void Logging::Plot(double value) {
  CriticalSectionScoped cs(crit_sect_.get());
  ThreadMap::iterator it = thread_map_.find(ThreadWrapper::GetThreadId());
  assert(it != thread_map_.end());
  const State& state = it->second.stack.top();
  if (state.enabled) {
    printf("PLOT\t%s\t%f\t%f\n", state.tag.c_str(), state.timestamp_ms * 0.001,
           value);
  }
}

Logging::Logging()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      thread_map_() {
}

Logging::State::State() : tag(""), timestamp_ms(0), enabled(true) {}

Logging::State::State(const std::string& tag, int64_t timestamp_ms,
                      bool enabled)
    : tag(tag),
      timestamp_ms(timestamp_ms),
      enabled(enabled) {
}

void Logging::State::MergePrevious(const State& previous) {
  if (tag == "") {
    tag = previous.tag;
  } else if (previous.tag != "") {
    tag = previous.tag + "_" + tag;
  }
  timestamp_ms = std::max(previous.timestamp_ms, timestamp_ms);
  enabled = previous.enabled && enabled;
}

void Logging::PushState(const std::string& append_to_tag, int64_t timestamp_ms,
                        bool enabled) {
  CriticalSectionScoped cs(crit_sect_.get());
  State new_state(append_to_tag, timestamp_ms, enabled);
  ThreadState* thread_state = &thread_map_[ThreadWrapper::GetThreadId()];
  std::stack<State>* stack = &thread_state->stack;
  if (stack->empty()) {
    new_state.MergePrevious(thread_state->global_state);
  } else {
    new_state.MergePrevious(stack->top());
  }
  stack->push(new_state);
}

void Logging::PopState() {
  CriticalSectionScoped cs(crit_sect_.get());
  ThreadMap::iterator it = thread_map_.find(ThreadWrapper::GetThreadId());
  assert(it != thread_map_.end());
  std::stack<State>* stack = &it->second.stack;
  int64_t newest_timestamp_ms = stack->top().timestamp_ms;
  stack->pop();
  if (!stack->empty()) {
    State* state = &stack->top();
    // Update time so that next log/plot will use the latest time seen so far
    // in this call tree.
    state->timestamp_ms = std::max(state->timestamp_ms, newest_timestamp_ms);
  }
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
