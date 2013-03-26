/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StartupTimeline.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsXULAppAPI.h"

namespace mozilla {

TimeStamp StartupTimeline::sStartupTimeline[StartupTimeline::MAX_EVENT_ID];
const char *StartupTimeline::sStartupTimelineDesc[StartupTimeline::MAX_EVENT_ID] = {
#define mozilla_StartupTimeline_Event(ev, desc) desc,
#include "StartupTimeline.h"
#undef mozilla_StartupTimeline_Event
};

/**
 * Implementation of XRE_StartupTimelineRecord()
 *
 * @param aEvent Same as XRE_StartupTimelineRecord() equivalent argument
 * @param aWhen  Same as XRE_StartupTimelineRecord() equivalent argument
 */
void
StartupTimelineRecordExternal(int aEvent, uint64_t aWhen)
{
#if XP_WIN
  TimeStamp ts = TimeStampValue(aWhen, 0, 0);
#else
  TimeStamp ts = TimeStampValue(aWhen);
#endif
  bool error = false;

  // Since the timestamp comes from an external source validate it before
  // recording it and log a telemetry error if it appears inconsistent.
  if (ts < TimeStamp::ProcessCreation(error)) {
    Telemetry::Accumulate(Telemetry::STARTUP_MEASUREMENT_ERRORS,
      (StartupTimeline::Event)aEvent);
  } else {
    StartupTimeline::Record((StartupTimeline::Event)aEvent, ts);
  }
}

} /* namespace mozilla */

/**
 * The XRE_StartupTimeline_Record function is to be used by embedding
 * applications that can't use mozilla::StartupTimeline::Record() directly.
 *
 * It can create timestamps from arbitrary time values and sanitizies them to
 * ensure that they are not inconsistent with those captured using monotonic
 * timers. The value of aWhen must have been captured using the same timer
 * used by the platform's mozilla::TimeStamp implementation. Erroneous values
 * will be flagged as telemetry errors.
 *
 * @param aEvent The event to be recorded, must correspond to an element of the
 *               mozilla::StartupTimeline::Event enumartion
 * @param aWhen  The time at which the event happened, must have been recorded
 *               using the same timer as the platform's mozilla::TimeStamp
 *               implementation
 */
void
XRE_StartupTimelineRecord(int aEvent, PRTime aWhen)
{
  mozilla::StartupTimelineRecordExternal(aEvent, aWhen);
}
