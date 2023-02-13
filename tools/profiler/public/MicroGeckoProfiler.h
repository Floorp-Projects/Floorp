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

#include <mozilla/Types.h>
#include <stdio.h>

#ifdef _WIN32
#  include <libloaderapi.h>
#else
#  include <dlfcn.h>
#endif

extern MOZ_EXPORT void uprofiler_register_thread(const char* aName,
                                                 void* aGuessStackTop);

extern MOZ_EXPORT void uprofiler_unregister_thread();

extern MOZ_EXPORT void uprofiler_simple_event_marker(
    const char* name, char phase, int num_args, const char** arg_names,
    const unsigned char* arg_types, const unsigned long long* arg_values);
#ifdef __cplusplus
}

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

void uprofiler_simple_event_marker(const char* name, char phase, int num_args,
                                   const char** arg_names,
                                   const unsigned char* arg_types,
                                   const unsigned long long* arg_values);

struct UprofilerFuncPtrs {
  void (*register_thread)(const char* aName, void* aGuessStackTop);
  void (*unregister_thread)();
  void (*simple_event_marker)(const char* name, char phase, int num_args,
                              const char** arg_names,
                              const unsigned char* arg_types,
                              const unsigned long long* arg_values);
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static void register_thread_noop(const char* aName, void* aGuessStackTop) {
  /* no-op */
}
static void unregister_thread_noop() { /* no-op */
}
static void simple_event_marker_noop(const char* name, char phase, int num_args,
                                     const char** arg_names,
                                     const unsigned char* arg_types,
                                     const unsigned long long* arg_values) {
  /* no-op */
}

#pragma GCC diagnostic pop

#if defined(_WIN32)
#  define UPROFILER_OPENLIB() GetModuleHandle(NULL)
#else
#  define UPROFILER_OPENLIB() dlopen(NULL, RTLD_NOW)
#endif

#if defined(_WIN32)
#  define UPROFILER_GET_SYM(handle, sym) GetProcAddress(handle, sym)
#else
#  define UPROFILER_GET_SYM(handle, sym) dlsym(handle, sym)
#endif

#if defined(_WIN32)
#  define UPROFILER_PRINT_ERROR(func) fprintf(stderr, "%s error\n", #func);
#else
#  define UPROFILER_PRINT_ERROR(func) \
    fprintf(stderr, "%s error: %s\n", #func, dlerror());
#endif

// Assumes that a variable of type UprofilerFuncPtrs, named uprofiler
// is accessible in the scope
#define UPROFILER_GET_FUNCTIONS()                                 \
  void* handle = UPROFILER_OPENLIB();                             \
  if (!handle) {                                                  \
    UPROFILER_PRINT_ERROR(UPROFILER_OPENLIB);                     \
    uprofiler.register_thread = register_thread_noop;             \
    uprofiler.unregister_thread = unregister_thread_noop;         \
    uprofiler.simple_event_marker = simple_event_marker_noop;     \
  }                                                               \
  uprofiler.register_thread =                                     \
      UPROFILER_GET_SYM(handle, "uprofiler_register_thread");     \
  if (!uprofiler.register_thread) {                               \
    UPROFILER_PRINT_ERROR(uprofiler_unregister_thread);           \
    uprofiler.register_thread = register_thread_noop;             \
  }                                                               \
  uprofiler.unregister_thread =                                   \
      UPROFILER_GET_SYM(handle, "uprofiler_unregister_thread");   \
  if (!uprofiler.unregister_thread) {                             \
    UPROFILER_PRINT_ERROR(uprofiler_unregister_thread);           \
    uprofiler.unregister_thread = unregister_thread_noop;         \
  }                                                               \
  uprofiler.simple_event_marker =                                 \
      UPROFILER_GET_SYM(handle, "uprofiler_simple_event_marker"); \
  if (!uprofiler.simple_event_marker) {                           \
    UPROFILER_PRINT_ERROR(uprofiler_simple_event_marker);         \
    uprofiler.simple_event_marker = simple_event_marker_noop;     \
  }

#endif  // MICRO_GECKO_PROFILER
