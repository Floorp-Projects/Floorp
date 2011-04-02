/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Robert O'Callahan <robert@ocallahan.org>
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
#include "prlock.h"

namespace mozilla {

struct TimeStampInitialization
{
  TimeStampInitialization() {
    TimeStamp::Startup();
  }
  ~TimeStampInitialization() {
    TimeStamp::Shutdown();
  }
};

static TimeStampInitialization initOnce;

static PRLock* gTimeStampLock = 0;
static PRUint32 gRolloverCount;
static PRIntervalTime gLastNow;

double
TimeDuration::ToSeconds() const
{
 return double(mValue)/PR_TicksPerSecond();
}

double
TimeDuration::ToSecondsSigDigits() const
{
  return ToSeconds();
}

TimeDuration
TimeDuration::FromMilliseconds(double aMilliseconds)
{
  static double kTicksPerMs = double(PR_TicksPerSecond()) / 1000.0;
  return TimeDuration::FromTicks(aMilliseconds * kTicksPerMs);
}

TimeDuration
TimeDuration::Resolution()
{
  // This is grossly nonrepresentative of actual system capabilities
  // on some platforms
  return TimeDuration::FromTicks(PRInt64(1));
}

nsresult
TimeStamp::Startup()
{
  if (gTimeStampLock)
    return NS_OK;

  // TimeStamp has to use bare PRLock instead of mozilla::Mutex
  // because TimeStamp can be used very early in startup.
  gTimeStampLock = PR_NewLock();
  gRolloverCount = 1;
  gLastNow = 0;
  return gTimeStampLock ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
TimeStamp::Shutdown()
{
  if (gTimeStampLock) {
    PR_DestroyLock(gTimeStampLock);
    gTimeStampLock = nsnull;
  }
}

TimeStamp
TimeStamp::Now()
{
  // XXX this could be considerably simpler and faster if we had
  // 64-bit atomic operations
  PR_Lock(gTimeStampLock);

  PRIntervalTime now = PR_IntervalNow();
  if (now < gLastNow) {
    ++gRolloverCount;
    // This can't happen unless you've been running for millions of years
    NS_ASSERTION(gRolloverCount > 0, "Rollover in rollover count???");
  }

  gLastNow = now;
  TimeStamp result((PRUint64(gRolloverCount) << 32) + now);

  PR_Unlock(gTimeStampLock);
  return result;
}

}
