/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";
import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

// Time out in just a single second. The task is set up to run for 5 minutes,
// so it should always time out.
export const backgroundTaskTimeoutSec = 1;

export async function runBackgroundTask() {
  await new Promise(resolve => {
    const fiveMinutesInMs = 5 * 60 * 1000;
    setTimeout(resolve, fiveMinutesInMs);
  });
  return EXIT_CODE.SUCCESS;
}
