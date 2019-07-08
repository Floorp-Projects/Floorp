/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AutoProfilerLabel.h"

#include "mozilla/PlatformMutex.h"

namespace mozilla {

// RAII class that encapsulates all shared static data, and enforces locking
// when accessing this data.
class MOZ_RAII AutoProfilerLabelData {
 public:
  AutoProfilerLabelData() { sAPLMutex.Lock(); }

  ~AutoProfilerLabelData() { sAPLMutex.Unlock(); }

  AutoProfilerLabelData(const AutoProfilerLabelData&) = delete;
  void operator=(const AutoProfilerLabelData&) = delete;

  const ProfilerLabelEnter& EnterCRef() const { return sEnter; }
  ProfilerLabelEnter& EnterRef() { return sEnter; }

  const ProfilerLabelExit& ExitCRef() const { return sExit; }
  ProfilerLabelExit& ExitRef() { return sExit; }

  const uint32_t& GenerationCRef() const { return sGeneration; }
  uint32_t& GenerationRef() { return sGeneration; }

 private:
  // Thin shell around mozglue PlatformMutex, for local internal use.
  // Does not preserve behavior in JS record/replay.
  class Mutex : private mozilla::detail::MutexImpl {
   public:
    Mutex()
        : mozilla::detail::MutexImpl(
              mozilla::recordreplay::Behavior::DontPreserve) {}
    void Lock() { mozilla::detail::MutexImpl::lock(); }
    void Unlock() { mozilla::detail::MutexImpl::unlock(); }
  };

  // Mutex protecting access to the following static members.
  static Mutex sAPLMutex;

  static ProfilerLabelEnter sEnter;
  static ProfilerLabelExit sExit;

  // Current "generation" of RegisterProfilerLabelEnterExit calls.
  static uint32_t sGeneration;
};

/* static */ AutoProfilerLabelData::Mutex AutoProfilerLabelData::sAPLMutex;
/* static */ ProfilerLabelEnter AutoProfilerLabelData::sEnter = nullptr;
/* static */ ProfilerLabelExit AutoProfilerLabelData::sExit = nullptr;
/* static */ uint32_t AutoProfilerLabelData::sGeneration = 0;

void RegisterProfilerLabelEnterExit(ProfilerLabelEnter aEnter,
                                    ProfilerLabelExit aExit) {
  MOZ_ASSERT(!aEnter == !aExit, "Must provide both null or both non-null");

  AutoProfilerLabelData data;
  MOZ_ASSERT(!aEnter != !data.EnterRef(),
             "Must go from null to non-null, or from non-null to null");
  data.EnterRef() = aEnter;
  data.ExitRef() = aExit;
  ++data.GenerationRef();
}

AutoProfilerLabel::AutoProfilerLabel(
    const char* aLabel,
    const char* aDynamicString MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  const AutoProfilerLabelData data;
  mEntryContext = (data.EnterCRef())
                      ? data.EnterCRef()(aLabel, aDynamicString, this)
                      : nullptr;
  mGeneration = data.GenerationCRef();
}

AutoProfilerLabel::~AutoProfilerLabel() {
  if (!mEntryContext) {
    return;
  }
  const AutoProfilerLabelData data;
  if (data.ExitCRef() && (mGeneration == data.GenerationCRef())) {
    data.ExitCRef()(mEntryContext);
  }
}

}  // namespace mozilla
