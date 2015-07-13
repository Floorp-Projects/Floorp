/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef mozilla_StartupTimeline_Event
mozilla_StartupTimeline_Event(PROCESS_CREATION, "process")
mozilla_StartupTimeline_Event(START, "start")
mozilla_StartupTimeline_Event(MAIN, "main")
mozilla_StartupTimeline_Event(SELECT_PROFILE, "selectProfile")
mozilla_StartupTimeline_Event(AFTER_PROFILE_LOCKED, "afterProfileLocked")
// Record the beginning and end of startup crash detection to compare with crash stats to know whether
// detection should be improved to start or end sooner.
mozilla_StartupTimeline_Event(STARTUP_CRASH_DETECTION_BEGIN, "startupCrashDetectionBegin")
mozilla_StartupTimeline_Event(STARTUP_CRASH_DETECTION_END, "startupCrashDetectionEnd")
mozilla_StartupTimeline_Event(FIRST_PAINT, "firstPaint")
mozilla_StartupTimeline_Event(SESSION_RESTORE_INIT, "sessionRestoreInit")
mozilla_StartupTimeline_Event(SESSION_RESTORED, "sessionRestored")
mozilla_StartupTimeline_Event(CREATE_TOP_LEVEL_WINDOW, "createTopLevelWindow")
mozilla_StartupTimeline_Event(LINKER_INITIALIZED, "linkerInitialized")
mozilla_StartupTimeline_Event(LIBRARIES_LOADED, "librariesLoaded")
mozilla_StartupTimeline_Event(FIRST_LOAD_URI, "firstLoadURI")

// The following are actually shutdown events, used to monitor the duration of shutdown
mozilla_StartupTimeline_Event(QUIT_APPLICATION, "quitApplication")
mozilla_StartupTimeline_Event(PROFILE_BEFORE_CHANGE, "profileBeforeChange")
#else

#ifndef mozilla_StartupTimeline
#define mozilla_StartupTimeline

#include "mozilla/TimeStamp.h"
#include "nscore.h"
#include "GeckoProfiler.h"

#ifdef MOZ_LINKER
extern "C" {
/* This symbol is resolved by the custom linker. The function it resolves
 * to dumps some statistics about the linker at the key events recorded
 * by the startup timeline. */
extern void __moz_linker_stats(const char *str)
NS_VISIBILITY_DEFAULT __attribute__((weak));
} /* extern "C" */
#else

#endif

namespace mozilla {

void RecordShutdownEndTimeStamp();
void RecordShutdownStartTimeStamp();

class StartupTimeline {
public:
  enum Event {
    #define mozilla_StartupTimeline_Event(ev, z) ev,
    #include "StartupTimeline.h"
    #undef mozilla_StartupTimeline_Event
    MAX_EVENT_ID
  };

  static TimeStamp Get(Event ev) {
    return sStartupTimeline[ev];
  }

  static const char *Describe(Event ev) {
    return sStartupTimelineDesc[ev];
  }

  static void Record(Event ev) {
    PROFILER_MARKER(Describe(ev));
    Record(ev, TimeStamp::Now());
  }

  static void Record(Event ev, TimeStamp when) {
    sStartupTimeline[ev] = when;
#ifdef MOZ_LINKER
    if (__moz_linker_stats)
      __moz_linker_stats(Describe(ev));
#endif
  }

  static void RecordOnce(Event ev) {
    if (!HasRecord(ev))
      Record(ev);
  }

  static bool HasRecord(Event ev) {
    return !sStartupTimeline[ev].IsNull();
  }

private:
  static NS_EXTERNAL_VIS_(TimeStamp) sStartupTimeline[MAX_EVENT_ID];
  static NS_EXTERNAL_VIS_(const char *) sStartupTimelineDesc[MAX_EVENT_ID];
};

} // namespace mozilla

#endif /* mozilla_StartupTimeline */

#endif /* mozilla_StartupTimeline_Event */
