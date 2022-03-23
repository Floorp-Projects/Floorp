/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the OS-independent methods of the TimeStamp class
 */

#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Uptime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace mozilla {

/**
 * Wrapper class used to initialize static data used by the TimeStamp class
 */
struct TimeStampInitialization {
  /**
   * First timestamp taken when the class static initializers are run. This
   * timestamp is used to sanitize timestamps coming from different sources.
   */
  TimeStamp mFirstTimeStamp;

  /**
   * Timestamp representing the time when the process was created. This field
   * is populated lazily the first time this information is required and is
   * replaced every time the process is restarted.
   */
  TimeStamp mProcessCreation;

  TimeStampInitialization() {
    TimeStamp::Startup();
    TimeStamp now = TimeStamp::Now();

    TimeStamp process_creation;
    char* mozAppRestart = getenv("MOZ_APP_RESTART");

    /* When calling PR_SetEnv() with an empty value the existing variable may
     * be unset or set to the empty string depending on the underlying platform
     * thus we have to check if the variable is present and not empty. */
    if (mozAppRestart && (strcmp(mozAppRestart, "") != 0)) {
      process_creation = now;
    } else {
      uint64_t uptime = TimeStamp::ComputeProcessUptime();
      process_creation =
          now - TimeDuration::FromMicroseconds(static_cast<double>(uptime));

      if ((process_creation > now) || (uptime == 0)) {
        process_creation = now;
      }
    }

    mFirstTimeStamp = now;
    mProcessCreation = process_creation;

    // On Windows < 10, initializing the uptime requires `mFirstTimeStamp` to be
    // valid.
    mozilla::InitializeUptime();
  };

  ~TimeStampInitialization() { TimeStamp::Shutdown(); };
};

static TimeStampInitialization sInitOnce;

MFBT_API TimeStamp TimeStamp::ProcessCreation() {
  return sInitOnce.mProcessCreation;
}

void TimeStamp::RecordProcessRestart() {
  sInitOnce.mProcessCreation = TimeStamp::Now();
}

}  // namespace mozilla
