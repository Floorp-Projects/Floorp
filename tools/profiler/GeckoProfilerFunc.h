/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_FUNCS_H
#define PROFILER_FUNCS_H

#include "mozilla/NullPtr.h"
#include "js/TypeDecls.h"
#include <stdint.h>

namespace mozilla {
class TimeDuration;
class TimeStamp;
}

using mozilla::TimeStamp;
using mozilla::TimeDuration;

class ProfilerBacktrace;
class ProfilerMarkerPayload;

// Returns a handle to pass on exit. This can check that we are popping the
// correct callstack.
inline void* mozilla_sampler_call_enter(const char *aInfo, void *aFrameAddress = nullptr,
                                        bool aCopy = false, uint32_t line = 0);
inline void  mozilla_sampler_call_exit(void* handle);
void  mozilla_sampler_add_marker(const char *aInfo,
                                 ProfilerMarkerPayload *aPayload = nullptr);

void mozilla_sampler_start(int aEntries, double aInterval,
                           const char** aFeatures, uint32_t aFeatureCount,
                           const char** aThreadNameFilters, uint32_t aFilterCount);

void mozilla_sampler_stop();

bool mozilla_sampler_is_paused();
void mozilla_sampler_pause();
void mozilla_sampler_resume();

ProfilerBacktrace* mozilla_sampler_get_backtrace();
void mozilla_sampler_free_backtrace(ProfilerBacktrace* aBacktrace);

bool mozilla_sampler_is_active();

void mozilla_sampler_responsiveness(const mozilla::TimeStamp& time);

void mozilla_sampler_frame_number(int frameNumber);

const double* mozilla_sampler_get_responsiveness();

void mozilla_sampler_save();

char* mozilla_sampler_get_profile();

JSObject *mozilla_sampler_get_profile_data(JSContext *aCx);

const char** mozilla_sampler_get_features();

void mozilla_sampler_init(void* stackTop);

void mozilla_sampler_shutdown();

void mozilla_sampler_print_location1();
void mozilla_sampler_print_location2();

// Lock the profiler. When locked the profiler is (1) stopped,
// (2) profile data is cleared, (3) profiler-locked is fired.
// This is used to lock down the profiler during private browsing
void mozilla_sampler_lock();

// Unlock the profiler, leaving it stopped and fires profiler-unlocked.
void mozilla_sampler_unlock();

// Register/unregister threads with the profiler
bool mozilla_sampler_register_thread(const char* name, void* stackTop);
void mozilla_sampler_unregister_thread();

double mozilla_sampler_time();
double mozilla_sampler_time(const mozilla::TimeStamp& aTime);

/* Returns true if env var SPS_NEW is set to anything, else false. */
extern bool sps_version2();

void mozilla_sampler_tracing(const char* aCategory, const char* aInfo,
                             TracingMetadata aMetaData);

#endif

