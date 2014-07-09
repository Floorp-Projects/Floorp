/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// System
#include <string>
#include <stdio.h>
#include <errno.h>
#include <ostream>
#include <fstream>
#include <sstream>

// Profiler
#include "PlatformMacros.h"
#include "GeckoProfiler.h"
#include "platform.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"
#include "ProfileEntry.h"
#include "SyncProfile.h"
#include "SaveProfileTask.h"
#include "UnwinderThread2.h"
#include "TableTicker.h"

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

// JS
#include "js/OldDebugAPI.h"

// This file's exports are listed in GeckoProfilerImpl.h.

/* These will be set to something sensible before we take the first
   sample. */
UnwMode sUnwindMode      = UnwINVALID;
int     sUnwindInterval  = 0;
int     sUnwindStackScan = 0;
int     sProfileEntries  = 0;

using std::string;
using namespace mozilla;

#if _MSC_VER
 #define snprintf _snprintf
#endif


////////////////////////////////////////////////////////////////////////
// BEGIN take samples.
// Everything in this section RUNS IN SIGHANDLER CONTEXT

// RUNS IN SIGHANDLER CONTEXT
static
void genProfileEntry(/*MODIFIED*/UnwinderThreadBuffer* utb,
                     volatile StackEntry &entry,
                     PseudoStack *stack, void *lastpc)
{
  int lineno = -1;

  // Add a pseudostack-entry start label
  utb__addEntry( utb, ProfileEntry('h', 'P') );
  // And the SP value, if it is non-zero
  if (entry.isCpp() && entry.stackAddress() != 0) {
    utb__addEntry( utb, ProfileEntry('S', entry.stackAddress()) );
  }

  // First entry has tagName 's' (start)
  // Check for magic pointer bit 1 to indicate copy
  const char* sampleLabel = entry.label();
  if (entry.isCopyLabel()) {
    // Store the string using 1 or more 'd' (dynamic) tags
    // that will happen to the preceding tag

    utb__addEntry( utb, ProfileEntry('c', "") );
    // Add one to store the null termination
    size_t strLen = strlen(sampleLabel) + 1;
    for (size_t j = 0; j < strLen;) {
      // Store as many characters in the void* as the platform allows
      char text[sizeof(void*)];
      for (size_t pos = 0; pos < sizeof(void*) && j+pos < strLen; pos++) {
        text[pos] = sampleLabel[j+pos];
      }
      j += sizeof(void*)/sizeof(char);
      // Cast to *((void**) to pass the text data to a void*
      utb__addEntry( utb, ProfileEntry('d', *((void**)(&text[0]))) );
    }
    if (entry.isJs()) {
      if (!entry.pc()) {
        // The JIT only allows the top-most entry to have a nullptr pc
        MOZ_ASSERT(&entry == &stack->mStack[stack->stackSize() - 1]);
        // If stack-walking was disabled, then that's just unfortunate
        if (lastpc) {
          jsbytecode *jspc = js::ProfilingGetPC(stack->mRuntime, entry.script(),
                                                lastpc);
          if (jspc) {
            lineno = JS_PCToLineNumber(nullptr, entry.script(), jspc);
          }
        }
      } else {
        lineno = JS_PCToLineNumber(nullptr, entry.script(), entry.pc());
      }
    } else {
      lineno = entry.line();
    }
  } else {
    utb__addEntry( utb, ProfileEntry('c', sampleLabel) );
    if (entry.isCpp()) {
      lineno = entry.line();
    }
  }
  if (lineno != -1) {
    utb__addEntry( utb, ProfileEntry('n', lineno) );
  }

  // Add a pseudostack-entry end label
  utb__addEntry( utb, ProfileEntry('h', 'Q') );
}

// RUNS IN SIGHANDLER CONTEXT
// Generate pseudo-backtrace entries and put them in |utb|, with
// the order outermost frame first.
void genPseudoBacktraceEntries(/*MODIFIED*/UnwinderThreadBuffer* utb,
                               PseudoStack *aStack, TickSample *sample)
{
  // Call genProfileEntry to generate tags for each profile
  // entry.  Each entry will be bounded by a 'h' 'P' tag to
  // mark the start and a 'h' 'Q' tag to mark the end.
  uint32_t nInStack = aStack->stackSize();
  for (uint32_t i = 0; i < nInStack; i++) {
    genProfileEntry(utb, aStack->mStack[i], aStack, nullptr);
  }
# ifdef ENABLE_SPS_LEAF_DATA
  if (sample) {
    utb__addEntry( utb, ProfileEntry('l', (void*)sample->pc) );
#   ifdef ENABLE_ARM_LR_SAVING
    utb__addEntry( utb, ProfileEntry('L', (void*)sample->lr) );
#   endif
  }
# endif
}

// RUNS IN SIGHANDLER CONTEXT
static
void populateBuffer(UnwinderThreadBuffer* utb, TickSample* sample,
                    UTB_RELEASE_FUNC releaseFunction, bool jankOnly)
{
  ThreadProfile& sampledThreadProfile = *sample->threadProfile;
  PseudoStack* stack = sampledThreadProfile.GetPseudoStack();

  /* Manufacture the ProfileEntries that we will give to the unwinder
     thread, and park them in |utb|. */
  bool recordSample = true;

  /* Don't process the PeudoStack's markers or honour jankOnly if we're
     immediately sampling the current thread. */
  if (!sample->isSamplingCurrentThread) {
    // LinkedUWTBuffers before markers
    UWTBufferLinkedList* syncBufs = stack->getLinkedUWTBuffers();
    while (syncBufs && syncBufs->peek()) {
      LinkedUWTBuffer* syncBuf = syncBufs->popHead();
      utb__addEntry(utb, ProfileEntry('B', syncBuf->GetBuffer()));
    }
    // Marker(s) come before the sample
    ProfilerMarkerLinkedList* pendingMarkersList = stack->getPendingMarkers();
    while (pendingMarkersList && pendingMarkersList->peek()) {
      ProfilerMarker* marker = pendingMarkersList->popHead();
      stack->addStoredMarker(marker);
      utb__addEntry( utb, ProfileEntry('m', marker) );
    }
    stack->updateGeneration(sampledThreadProfile.GetGenerationID());
    if (jankOnly) {
      // if we are on a different event we can discard any temporary samples
      // we've kept around
      if (sLastSampledEventGeneration != sCurrentEventGeneration) {
        // XXX: we also probably want to add an entry to the profile to help
        // distinguish which samples are part of the same event. That, or record
        // the event generation in each sample
        sampledThreadProfile.erase();
      }
      sLastSampledEventGeneration = sCurrentEventGeneration;

      recordSample = false;
      // only record the events when we have a we haven't seen a tracer
      // event for 100ms
      if (!sLastTracerEvent.IsNull()) {
        mozilla::TimeDuration delta = sample->timestamp - sLastTracerEvent;
        if (delta.ToMilliseconds() > 100.0) {
            recordSample = true;
        }
      }
    }
  }

  // JRS 2012-Sept-27: this logic used to involve mUseStackWalk.
  // That should be reinstated, but for the moment, use the
  // settings in sUnwindMode and sUnwindInterval.
  // Add a native-backtrace request, or add pseudo backtrace entries,
  // or both.
  switch (sUnwindMode) {
    case UnwNATIVE: /* Native only */
      // add a "do native stack trace now" hint.  This will be actioned
      // by the unwinder thread as it processes the entries in this
      // sample.
      utb__addEntry( utb, ProfileEntry('h'/*hint*/, 'N'/*native-trace*/) );
      break;
    case UnwPSEUDO: /* Pseudo only */
      /* Add into |utb|, the pseudo backtrace entries */
      genPseudoBacktraceEntries(utb, stack, sample);
      break;
    case UnwCOMBINED: /* Both Native and Pseudo */
      utb__addEntry( utb, ProfileEntry('h'/*hint*/, 'N'/*native-trace*/) );
      genPseudoBacktraceEntries(utb, stack, sample);
      break;
    case UnwINVALID:
    default:
      MOZ_CRASH();
  }

  if (recordSample) {
    // add a "flush now" hint
    utb__addEntry( utb, ProfileEntry('h'/*hint*/, 'F'/*flush*/) );
  }

  // Add any extras
  if (!sLastTracerEvent.IsNull() && sample) {
    mozilla::TimeDuration delta = sample->timestamp - sLastTracerEvent;
    utb__addEntry( utb, ProfileEntry('r', static_cast<float>(delta.ToMilliseconds())) );
  }

  if (sample) {
    mozilla::TimeDuration delta = sample->timestamp - sStartTime;
    utb__addEntry( utb, ProfileEntry('t', static_cast<float>(delta.ToMilliseconds())) );
  }

  if (sLastFrameNumber != sFrameNumber) {
    utb__addEntry( utb, ProfileEntry('f', sFrameNumber) );
    sLastFrameNumber = sFrameNumber;
  }

  /* So now we have, in |utb|, the complete set of entries we want to
     push into the circular buffer.  This may also include a 'h' 'F'
     entry, which is "flush now" hint, and/or a 'h' 'N' entry, which
     is a "generate a native backtrace and add it to the buffer right
     now" hint.  Hand them off to the helper thread, together with
     stack and register context needed to do a native unwind, if that
     is currently enabled. */

  /* If a native unwind has been requested, we'll start it off using
     the context obtained from the signal handler, to avoid the
     problem of having to unwind through the signal frame itself. */

  /* On Linux and Android, the initial register state is in the
     supplied sample->context.  But on MacOS it's not, so we have to
     fake it up here (sigh). */
  if (sUnwindMode == UnwNATIVE || sUnwindMode == UnwCOMBINED) {
#   if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
       || defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android)
    void* ucV = (void*)sample->context;
#   elif defined(SPS_PLAT_amd64_darwin)
    struct __darwin_mcontext64 mc;
    memset(&mc, 0, sizeof(mc));
    ucontext_t uc;
    memset(&uc, 0, sizeof(uc));
    uc.uc_mcontext = &mc;
    mc.__ss.__rip = (uint64_t)sample->pc;
    mc.__ss.__rsp = (uint64_t)sample->sp;
    mc.__ss.__rbp = (uint64_t)sample->fp;
    void* ucV = (void*)&uc;
#   elif defined(SPS_PLAT_x86_darwin)
    struct __darwin_mcontext32 mc;
    memset(&mc, 0, sizeof(mc));
    ucontext_t uc;
    memset(&uc, 0, sizeof(uc));
    uc.uc_mcontext = &mc;
    mc.__ss.__eip = (uint32_t)sample->pc;
    mc.__ss.__esp = (uint32_t)sample->sp;
    mc.__ss.__ebp = (uint32_t)sample->fp;
    void* ucV = (void*)&uc;
#   elif defined(SPS_OS_windows)
    /* Totally fake this up so it at least builds.  No idea if we can
       even ever get here on Windows. */
    void* ucV = nullptr;
#   else
#     error "Unsupported platform"
#   endif
    releaseFunction(&sampledThreadProfile, utb, ucV);
  } else {
    releaseFunction(&sampledThreadProfile, utb, nullptr);
  }
}

static
void sampleCurrent(TickSample* sample)
{
  // This variant requires sample->threadProfile to be set
  MOZ_ASSERT(sample->threadProfile);
  LinkedUWTBuffer* syncBuf = utb__acquire_sync_buffer(tlsStackTop.get());
  if (!syncBuf) {
    return;
  }
  SyncProfile* syncProfile = sample->threadProfile->AsSyncProfile();
  MOZ_ASSERT(syncProfile);
  if (!syncProfile->SetUWTBuffer(syncBuf)) {
    utb__release_sync_buffer(syncBuf);
    return;
  }
  UnwinderThreadBuffer* utb = syncBuf->GetBuffer();
  populateBuffer(utb, sample, &utb__finish_sync_buffer, false);
}

// RUNS IN SIGHANDLER CONTEXT
void TableTicker::UnwinderTick(TickSample* sample)
{
  if (sample->isSamplingCurrentThread) {
    sampleCurrent(sample);
    return;
  }

  if (!sample->threadProfile) {
    // Platform doesn't support multithread, so use the main thread profile we created
    sample->threadProfile = GetPrimaryThreadProfile();
  }

  /* Get hold of an empty inter-thread buffer into which to park
     the ProfileEntries for this sample. */
  UnwinderThreadBuffer* utb = uwt__acquire_empty_buffer();

  /* This could fail, if no buffers are currently available, in which
     case we must give up right away.  We cannot wait for a buffer to
     become available, as that risks deadlock. */
  if (!utb)
    return;

  populateBuffer(utb, sample, &uwt__release_full_buffer, mJankOnly);
}

// END take samples
////////////////////////////////////////////////////////////////////////

