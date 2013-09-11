/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the OS-independent methods of the TimeStamp class
 */

#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIAppStartup.h"

namespace mozilla {

TimeStamp TimeStamp::sFirstTimeStamp;
TimeStamp TimeStamp::sProcessCreation;

TimeStamp
TimeStamp::ProcessCreation(bool& aIsInconsistent)
{
  aIsInconsistent = false;

  if (sProcessCreation.IsNull()) {
    TimeStamp ts;

    // Ask the startup service whether the app was restarted.
    nsCOMPtr<nsIAppStartup> appService =
      do_GetService("@mozilla.org/toolkit/app-startup;1");
    bool wasRestarted;
    appService->GetWasRestarted(&wasRestarted);

    if (wasRestarted) {
      /* Firefox was restarted, use the first time-stamp we've
       * taken as the new process startup time. */
      ts = sFirstTimeStamp;
    } else {
      TimeStamp now = Now();
      uint64_t uptime = ComputeProcessUptime();

      ts = now - TimeDuration::FromMicroseconds(uptime);

      if ((ts > sFirstTimeStamp) || (uptime == 0)) {
        /* If the process creation timestamp was inconsistent replace it with
         * the first one instead and notify that a telemetry error was
         * detected. */
        aIsInconsistent = true;
        ts = sFirstTimeStamp;
      }
    }

    sProcessCreation = ts;
  }

  return sProcessCreation;
}

void
TimeStamp::RecordProcessRestart()
{
  sProcessCreation = TimeStamp();
}

} // namespace mozilla
