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

#ifdef MOZILLA_INTERNAL_API
#include "GeckoProfiler.h"
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

#ifdef MOZILLA_INTERNAL_API
  static void Record(Event ev) {
    profiler_add_marker(Describe(ev));
    Record(ev, TimeStamp::Now());
  }

  static void Record(Event ev, TimeStamp when) {
    sStartupTimeline[ev] = when;
  }

  static void RecordOnce(Event ev) {
    if (!HasRecord(ev))
      Record(ev);
  }
#endif

  static bool HasRecord(Event ev) {
    return !sStartupTimeline[ev].IsNull();
  }

private:
  static TimeStamp sStartupTimeline[MAX_EVENT_ID];
  static const char* sStartupTimelineDesc[MAX_EVENT_ID];
};

} // namespace mozilla

#endif /* mozilla_StartupTimeline */

#endif /* mozilla_StartupTimeline_Event */
