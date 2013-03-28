/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StartupTimeline.h"
#include "nsXULAppAPI.h"

namespace mozilla {

TimeStamp StartupTimeline::sStartupTimeline[StartupTimeline::MAX_EVENT_ID];
const char *StartupTimeline::sStartupTimelineDesc[StartupTimeline::MAX_EVENT_ID] = {
#define mozilla_StartupTimeline_Event(ev, desc) desc,
#include "StartupTimeline.h"
#undef mozilla_StartupTimeline_Event
};

} /* namespace mozilla */

/**
 * The XRE_StartupTimeline_Record function is to be used by embedding applications
 * that can't use mozilla::StartupTimeline::Record() directly.
 */
void
XRE_StartupTimelineRecord(int aEvent, PRTime aWhen)
{
  /* FIXME: This effectively becomes a no-op until we fix all the users to
   * provide proper timestamps */
  mozilla::StartupTimeline::Record((mozilla::StartupTimeline::Event) aEvent,
    mozilla::TimeStamp());
}
