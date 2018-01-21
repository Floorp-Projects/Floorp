/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VTuneProfiler_h
#define VTuneProfiler_h

// The intent here is to add 0 overhead for regular users. In order to build
// the VTune profiler code at all --enable-vtune-instrumentation needs to be
// set as a build option. Even then, when none of the environment variables
// is specified that allow us to find the ittnotify DLL, these functions
// should be minimal overhead. When starting Firefox under VTune, these
// env vars will be automatically defined, otherwise INTEL_LIBITTNOTIFY32/64
// should be set to point at the ittnotify DLL.
#ifndef MOZ_VTUNE_INSTRUMENTATION

#define VTUNE_INIT()
#define VTUNE_SHUTDOWN()

#define VTUNE_TRACING(name, kind)
#define VTUNE_REGISTER_THREAD(name)

#else

#include <stddef.h>
#include <unordered_map>
#include <string>

#include "GeckoProfiler.h"

// This is the regular Intel header, these functions are actually defined for
// us inside js/src/vtune by an intel C file which actually dynamically resolves
// them to the correct DLL. Through libxul these will 'magically' resolve.
#include "vtune/ittnotify.h"

class VTuneProfiler
{
public:
  static void Initialize();
  static void Shutdown();

  static void Trace(const char* aName, TracingKind aKind)
  {
    if (mInstance) {
      mInstance->TraceInternal(aName, aKind);
    }
  }
  static void RegisterThread(const char* aName)
  {
    if (mInstance)
    {
      mInstance->RegisterThreadInternal(aName);
    }
  }

private:
  void TraceInternal(const char* aName, TracingKind aKind);
  void RegisterThreadInternal(const char* aName);

  // This is null when the ittnotify DLL could not be found.
  static VTuneProfiler* mInstance;

  std::unordered_map<std::string, __itt_event> mStrings;
};

#define VTUNE_INIT() VTuneProfiler::Initialize()
#define VTUNE_SHUTDOWN() VTuneProfiler::Shutdown()

#define VTUNE_TRACING(name, kind) VTuneProfiler::Trace(name, kind)
#define VTUNE_REGISTER_THREAD(name) VTuneProfiler::RegisterThread(name)

#endif

#endif /* VTuneProfiler_h */
