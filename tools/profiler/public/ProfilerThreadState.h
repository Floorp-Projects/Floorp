/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header contains functions that give information about the Profiler state
// with regards to the current thread.

#ifndef ProfilerThreadState_h
#define ProfilerThreadState_h

#include "mozilla/ProfilerState.h"
#include "mozilla/ProfilerThreadRegistration.h"
#include "mozilla/ProfilerThreadRegistry.h"

// During profiling, if the current thread is registered, return true
// (regardless of whether it is actively being profiled).
// (Same caveats and recommented usage as profiler_is_active().)
[[nodiscard]] inline bool profiler_is_active_and_thread_is_registered() {
  return profiler_is_active() &&
         mozilla::profiler::ThreadRegistration::IsRegistered();
}

// Is the profiler active and unpaused, and is the current thread being
// profiled? (Same caveats and recommented usage as profiler_is_active().)
[[nodiscard]] inline bool profiler_thread_is_being_profiled() {
  return profiler_is_active_and_unpaused() &&
         mozilla::profiler::ThreadRegistration::WithOnThreadRefOr(
             [](mozilla::profiler::ThreadRegistration::OnThreadRef aTR) {
               return aTR.UnlockedConstReaderAndAtomicRWCRef()
                   .IsBeingProfiled();
             },
             false);
}

// Is the profiler active and unpaused, and is the given thread being profiled?
// (Same caveats and recommented usage as profiler_is_active().)
// Safe to use with the current thread id, or unspecified ProfilerThreadId (same
// as current thread id).
[[nodiscard]] inline bool profiler_thread_is_being_profiled(
    const ProfilerThreadId& aThreadId) {
  if (!profiler_is_active_and_unpaused()) {
    return false;
  }

  if (!aThreadId.IsSpecified() || aThreadId == profiler_current_thread_id()) {
    // For the current thread id, use the ThreadRegistration directly, it is
    // more efficient.
    return mozilla::profiler::ThreadRegistration::WithOnThreadRefOr(
        [](mozilla::profiler::ThreadRegistration::OnThreadRef aTR) {
          return aTR.UnlockedConstReaderAndAtomicRWCRef().IsBeingProfiled();
        },
        false);
  }

  // For other threads, go through the ThreadRegistry.
  return mozilla::profiler::ThreadRegistry::WithOffThreadRefOr(
      aThreadId,
      [](mozilla::profiler::ThreadRegistry::OffThreadRef aTR) {
        return aTR.UnlockedConstReaderAndAtomicRWCRef().IsBeingProfiled();
      },
      false);
}

// Is the current thread registered and sleeping?
[[nodiscard]] inline bool profiler_thread_is_sleeping() {
  return profiler_is_active() &&
         mozilla::profiler::ThreadRegistration::WithOnThreadRefOr(
             [](mozilla::profiler::ThreadRegistration::OnThreadRef aTR) {
               return aTR.UnlockedConstReaderAndAtomicRWCRef().IsSleeping();
             },
             false);
}

#endif  // ProfilerThreadState_h
