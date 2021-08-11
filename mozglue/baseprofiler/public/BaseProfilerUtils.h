/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseProfilerUtils_h
#define BaseProfilerUtils_h

// This header contains most process- and thread-related functions.
// It is safe to include unconditionally.

#include <type_traits>

namespace mozilla::baseprofiler {

// Trivially-copyable class containing a process id. It may be left unspecified.
class BaseProfilerProcessId {
 public:
  // Unspecified process id.
  constexpr BaseProfilerProcessId() = default;

  [[nodiscard]] constexpr bool IsSpecified() const {
    return mProcessId != scUnspecified;
  }

  using NumberType = int;

  // Get the process id as a number, which may be unspecified.
  // This should only be used for serialization or logging.
  [[nodiscard]] constexpr NumberType ToNumber() const { return mProcessId; }

  // BaseProfilerProcessId from given number (which may be unspecified).
  constexpr static BaseProfilerProcessId FromNumber(
      const NumberType& aProcessId) {
    return BaseProfilerProcessId{aProcessId};
  }

  [[nodiscard]] constexpr bool operator==(
      const BaseProfilerProcessId& aOther) const {
    return mProcessId == aOther.mProcessId;
  }
  [[nodiscard]] constexpr bool operator!=(
      const BaseProfilerProcessId& aOther) const {
    return mProcessId != aOther.mProcessId;
  }

 private:
  constexpr explicit BaseProfilerProcessId(const NumberType& aProcessId)
      : mProcessId(aProcessId) {}

  static constexpr NumberType scUnspecified = 0;
  NumberType mProcessId = scUnspecified;
};

// Check traits. These should satisfy usage in std::atomic.
static_assert(std::is_trivially_copyable_v<BaseProfilerProcessId>);
static_assert(std::is_copy_constructible_v<BaseProfilerProcessId>);
static_assert(std::is_move_constructible_v<BaseProfilerProcessId>);
static_assert(std::is_copy_assignable_v<BaseProfilerProcessId>);
static_assert(std::is_move_assignable_v<BaseProfilerProcessId>);

// Trivially-copyable class containing a thread id. It may be left unspecified.
class BaseProfilerThreadId {
 public:
  // Unspecified thread id.
  constexpr BaseProfilerThreadId() = default;

  [[nodiscard]] constexpr bool IsSpecified() const {
    return mThreadId != scUnspecified;
  }

  using NumberType = int;

  // Get the thread id as a number, which may be unspecified.
  // This should only be used for serialization or logging.
  [[nodiscard]] constexpr NumberType ToNumber() const { return mThreadId; }

  // BaseProfilerThreadId from given number (which may be unspecified).
  constexpr static BaseProfilerThreadId FromNumber(
      const NumberType& aThreadId) {
    return BaseProfilerThreadId{aThreadId};
  }

  [[nodiscard]] constexpr bool operator==(
      const BaseProfilerThreadId& aOther) const {
    return mThreadId == aOther.mThreadId;
  }
  [[nodiscard]] constexpr bool operator!=(
      const BaseProfilerThreadId& aOther) const {
    return mThreadId != aOther.mThreadId;
  }

 private:
  constexpr explicit BaseProfilerThreadId(const NumberType& aThreadId)
      : mThreadId(aThreadId) {}

  static constexpr NumberType scUnspecified = 0;
  NumberType mThreadId = scUnspecified;
};

// Check traits. These should satisfy usage in std::atomic.
static_assert(std::is_trivially_copyable_v<BaseProfilerThreadId>);
static_assert(std::is_copy_constructible_v<BaseProfilerThreadId>);
static_assert(std::is_move_constructible_v<BaseProfilerThreadId>);
static_assert(std::is_copy_assignable_v<BaseProfilerThreadId>);
static_assert(std::is_move_assignable_v<BaseProfilerThreadId>);

}  // namespace mozilla::baseprofiler

#include "mozilla/Types.h"

namespace mozilla::baseprofiler {

// Get the current process's ID.
[[nodiscard]] MFBT_API BaseProfilerProcessId profiler_current_process_id();

// Get the current thread's ID.
[[nodiscard]] MFBT_API BaseProfilerThreadId profiler_current_thread_id();

// Must be called at least once from the main thread, before any other main-
// thread id function.
MFBT_API void profiler_init_main_thread_id();

[[nodiscard]] MFBT_API BaseProfilerThreadId profiler_main_thread_id();

[[nodiscard]] MFBT_API bool profiler_is_main_thread();

}  // namespace mozilla::baseprofiler

#endif  // BaseProfilerUtils_h
