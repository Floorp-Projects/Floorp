/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VTuneProfiler.h"
#include "mozilla/Bootstrap.h"
#include <memory>

using namespace std;

VTuneProfiler* VTuneProfiler::mInstance = nullptr;

void
VTuneProfiler::Initialize()
{
  // This is just a 'dirty trick' to find out if the ittnotify DLL was found.
  // If it wasn't this function always returns 0, otherwise it returns incrementing
  // numbers, if the library was found this wastes 2 events but that should be okay.
  __itt_event testEvent = __itt_event_createA("Test event", strlen("Test event"));
  testEvent = __itt_event_createA("Test event 2", strlen("Test event 2"));

  if (testEvent) {
    mInstance = new VTuneProfiler();
  }
}

void
VTuneProfiler::Shutdown()
{
}

void
VTuneProfiler::TraceInternal(const char* aName, TracingKind aKind)
{
  if (aKind == TRACING_EVENT) {
    return;
  }

  string str(aName);

  auto iter = mStrings.find(str);

  __itt_event event;
  if (iter != mStrings.end()) {
    event = iter->second;
  } else {
    event = __itt_event_createA(aName, str.length());
    mStrings.insert({ str, event });
  }

  if (aKind == TRACING_INTERVAL_START) {
    __itt_event_start(event);
  } else {
    __itt_event_end(event);
  }
}

void
VTuneProfiler::RegisterThreadInternal(const char* aName)
{
  string str(aName);

  if (!str.compare("GeckoMain")) {
    // Process main thread.
    switch (XRE_GetProcessType()) {
      case GeckoProcessType::GeckoProcessType_Default:
        __itt_thread_set_nameA("Main Process");
        break;
      case GeckoProcessType::GeckoProcessType_Content:
        __itt_thread_set_nameA("Content Process");
        break;
      case GeckoProcessType::GeckoProcessType_GMPlugin:
        __itt_thread_set_nameA("Plugin Process");
        break;
      case GeckoProcessType::GeckoProcessType_GPU:
        __itt_thread_set_nameA("GPU Process");
        break;
      default:
        __itt_thread_set_nameA("Unknown Process");
    }
    return;
  }
  __itt_thread_set_nameA(aName);
}