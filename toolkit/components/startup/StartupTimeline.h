/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef mozilla_StartupTimeline_Event
mozilla_StartupTimeline_Event(PROCESS_CREATION, "process")
mozilla_StartupTimeline_Event(MAIN, "main")
// Record the beginning and end of startup crash detection to compare with crash stats to know whether
// detection should be improved to start or end sooner.
mozilla_StartupTimeline_Event(STARTUP_CRASH_DETECTION_BEGIN, "startupCrashDetectionBegin")
mozilla_StartupTimeline_Event(STARTUP_CRASH_DETECTION_END, "startupCrashDetectionEnd")
mozilla_StartupTimeline_Event(FIRST_PAINT, "firstPaint")
mozilla_StartupTimeline_Event(SESSION_RESTORED, "sessionRestored")
mozilla_StartupTimeline_Event(CREATE_TOP_LEVEL_WINDOW, "createTopLevelWindow")
mozilla_StartupTimeline_Event(LINKER_INITIALIZED, "linkerInitialized")
mozilla_StartupTimeline_Event(LIBRARIES_LOADED, "librariesLoaded")
mozilla_StartupTimeline_Event(FIRST_LOAD_URI, "firstLoadURI")
#else

#ifndef mozilla_StartupTimeline
#define mozilla_StartupTimeline

#include "prtime.h"
#include "nscore.h"

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

class StartupTimeline {
public:
  enum Event {
    #define mozilla_StartupTimeline_Event(ev, z) ev,
    #include "StartupTimeline.h"
    #undef mozilla_StartupTimeline_Event
    MAX_EVENT_ID
  };

  static PRTime Get(Event ev) {
    return sStartupTimeline[ev];
  }

  static const char *Describe(Event ev) {
    return sStartupTimelineDesc[ev];
  }

  static void Record(Event ev, PRTime when = PR_Now()) {
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
    return sStartupTimeline[ev];
  }

private:
  static NS_EXTERNAL_VIS_(PRTime) sStartupTimeline[MAX_EVENT_ID];
  static NS_EXTERNAL_VIS_(const char *) sStartupTimelineDesc[MAX_EVENT_ID];
};

}

#endif /* mozilla_StartupTimeline */

#endif /* mozilla_StartupTimeline_Event */
