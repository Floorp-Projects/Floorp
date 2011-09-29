/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is XPCOM TimeStamp tests.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   robert@ocallahan.org
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

#include "mozilla/TimeStamp.h"

#include "TestHarness.h"

#include "prinrval.h"
#include "prthread.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

static void Assert(bool aCond, const char* aMsg)
{
    if (aCond) {
        passed(aMsg);
    } else {
        fail("%s", aMsg);
    }
}

int main(int argc, char** argv)
{
    ScopedXPCOM xpcom("nsTimeStamp");
    if (xpcom.failed())
        return 1;

    TimeDuration td;
    Assert(td.ToSeconds() == 0.0, "TimeDuration default value 0");
    Assert(TimeDuration::FromSeconds(5).ToSeconds() == 5.0,
           "TimeDuration FromSeconds/ToSeconds round-trip");
    Assert(TimeDuration::FromMilliseconds(5000).ToSeconds() == 5.0,
           "TimeDuration FromMilliseconds/ToSeconds round-trip");
    Assert(TimeDuration::FromSeconds(1) < TimeDuration::FromSeconds(2),
           "TimeDuration < operator works");
    Assert(!(TimeDuration::FromSeconds(1) < TimeDuration::FromSeconds(1)),
           "TimeDuration < operator works");
    Assert(TimeDuration::FromSeconds(2) > TimeDuration::FromSeconds(1),
           "TimeDuration > operator works");
    Assert(!(TimeDuration::FromSeconds(1) > TimeDuration::FromSeconds(1)),
           "TimeDuration > operator works");
    Assert(TimeDuration::FromSeconds(1) <= TimeDuration::FromSeconds(2),
           "TimeDuration <= operator works");
    Assert(TimeDuration::FromSeconds(1) <= TimeDuration::FromSeconds(1),
           "TimeDuration <= operator works");
    Assert(!(TimeDuration::FromSeconds(2) <= TimeDuration::FromSeconds(1)),
           "TimeDuration <= operator works");
    Assert(TimeDuration::FromSeconds(2) >= TimeDuration::FromSeconds(1),
           "TimeDuration >= operator works");
    Assert(TimeDuration::FromSeconds(1) >= TimeDuration::FromSeconds(1),
           "TimeDuration >= operator works");
    Assert(!(TimeDuration::FromSeconds(1) >= TimeDuration::FromSeconds(2)),
           "TimeDuration >= operator works");

    TimeStamp ts;
    Assert(ts.IsNull(), "Default TimeStamp value null");
    
    ts = TimeStamp::Now();
    Assert(!ts.IsNull(), "TimeStamp time value non-null");
    Assert((ts - ts).ToSeconds() == 0.0, "TimeStamp zero-length duration");

    PR_Sleep(PR_SecondsToInterval(2));

    TimeStamp ts2(TimeStamp::Now());
    Assert(ts2 > ts, "TimeStamp > comparison");
    Assert(!(ts > ts), "TimeStamp > comparison");
    Assert(ts < ts2, "TimeStamp < comparison");
    Assert(!(ts < ts), "TimeStamp < comparison");
    Assert(ts <= ts2, "TimeStamp <= comparison");
    Assert(ts <= ts, "TimeStamp <= comparison");
    Assert(!(ts2 <= ts), "TimeStamp <= comparison");
    Assert(ts2 >= ts, "TimeStamp >= comparison");
    Assert(ts2 >= ts, "TimeStamp >= comparison");
    Assert(!(ts >= ts2), "TimeStamp >= comparison");

    // We can't be sure exactly how long PR_Sleep slept for. It should have
    // slept for at least one second. We might have slept a lot longer due
    // to process scheduling, but hopefully not more than 10 seconds.
    td = ts2 - ts;
    Assert(td.ToSeconds() > 1.0, "TimeStamp difference lower bound");
    Assert(td.ToSeconds() < 20.0, "TimeStamp difference upper bound");
    td = ts - ts2;
    Assert(td.ToSeconds() < -1.0, "TimeStamp negative difference lower bound");
    Assert(td.ToSeconds() > -20.0, "TimeStamp negative difference upper bound");

    double resolution = TimeDuration::Resolution().ToSecondsSigDigits();
    printf(" (platform timer resolution is ~%g s)\n", resolution);
    Assert(0.000000001 < resolution, "Time resolution is sane");
    // Don't upper-bound sanity check ... although NSPR reports 1ms
    // resolution, it might be lying, so we shouldn't compare with it

    return gFailCount > 0;
}
