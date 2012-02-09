/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
