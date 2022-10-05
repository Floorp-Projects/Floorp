/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This contains things related to the Gecko profiler, for use in third_party
// code. It is very minimal and is designed to be used by patching over
// upstream code.
// Only use the C ABI and guard C++ code with #ifdefs, don't pull anything from
// Gecko, it must be possible to include the header file into any C++ codebase.

#ifndef MICRO_GECKO_PROFILER
#define MICRO_GECKO_PROFILER

#ifdef __cplusplus
extern "C" {
#endif

void uprofiler_register_thread(const char* aName, void* aGuessStackTop);
void uprofiler_unregister_thread();
void uprofiler_simple_event_marker(const char* name, char phase, int num_args,
                                   const char** arg_names,
                                   const unsigned char* arg_types,
                                   const unsigned long long* arg_values);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
struct AutoRegisterProfiler {
  AutoRegisterProfiler(const char* name, char* stacktop) {
    if (getenv("MOZ_UPROFILER_LOG_THREAD_CREATION")) {
      printf("### UProfiler: new thread: '%s'\n", name);
    }
    uprofiler_register_thread(name, stacktop);
  }
  ~AutoRegisterProfiler() { uprofiler_unregister_thread(); }
};
#endif  // __cplusplus

#endif  // MICRO_GECKO_PROFILER
