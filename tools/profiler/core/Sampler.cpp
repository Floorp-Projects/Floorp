/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <string>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "GeckoProfiler.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prtime.h"
#include "nsXULAppAPI.h"
#include "ProfileEntry.h"
#include "SyncProfile.h"
#include "platform.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"

// JSON
#include "ProfileJSONWriter.h"

// Meta
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIHttpProtocolHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsIXULAppInfo.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "PlatformMacros.h"
#include "nsTArray.h"

#include "mozilla/Preferences.h"

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "FennecJNIWrappers.h"
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif // MOZ_TASK_TRACER

// JS
#include "jsfriendapi.h"
#include "js/ProfilingFrameIterator.h"

using std::string;
using namespace mozilla;

#ifndef MAXPATHLEN
 #ifdef PATH_MAX
  #define MAXPATHLEN PATH_MAX
 #elif defined(MAX_PATH)
  #define MAXPATHLEN MAX_PATH
 #elif defined(_MAX_PATH)
  #define MAXPATHLEN _MAX_PATH
 #elif defined(CCHMAXPATH)
  #define MAXPATHLEN CCHMAXPATH
 #else
  #define MAXPATHLEN 1024
 #endif
#endif

Sampler::Sampler()
{
  MOZ_COUNT_CTOR(Sampler);

  bool ignore;
  sStartTime = mozilla::TimeStamp::ProcessCreation(ignore);

  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    // Set up profiling for each registered thread, if appropriate
    for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
      ThreadInfo* info = sRegisteredThreads->at(i);

      MaybeSetProfile(info);
    }
  }

#ifdef MOZ_TASK_TRACER
  if (mTaskTracer) {
    mozilla::tasktracer::StartLogging();
  }
#endif
}

Sampler::~Sampler()
{
  MOZ_COUNT_DTOR(Sampler);

  if (gIsActive)
    PlatformStop();

  // Destroy ThreadInfo for all threads
  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
      ThreadInfo* info = sRegisteredThreads->at(i);
      // We've stopped profiling. We no longer need to retain
      // information for an old thread.
      if (info->IsPendingDelete()) {
        // The stack was nulled when SetPendingDelete() was called.
        MOZ_ASSERT(!info->Stack());
        delete info;
        sRegisteredThreads->erase(sRegisteredThreads->begin() + i);
        i--;
      }
    }
  }

#ifdef MOZ_TASK_TRACER
  if (mTaskTracer) {
    mozilla::tasktracer::StopLogging();
  }
#endif
}

void PseudoStack::flushSamplerOnJSShutdown()
{
  MOZ_ASSERT(mContext);

  if (!gIsActive) {
    return;
  }

  gIsPaused = true;

  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread not being profiled, skip it.
      ThreadInfo* info = sRegisteredThreads->at(i);
      if (!info->hasProfile() || info->IsPendingDelete()) {
        continue;
      }

      // Thread not profiling the context that's going away, skip it.
      if (info->Stack()->mContext != mContext) {
        continue;
      }

      MutexAutoLock lock(info->GetMutex());
      info->FlushSamplesAndMarkers();
    }
  }

  gIsPaused = false;
}

// END SaveProfileTask et al
////////////////////////////////////////////////////////////////////////

