/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["backgroundTaskTimeoutSec", "runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

// Time out in just a single second. The task is set up to run for 5 minutes,
// so it should always time out.
const backgroundTaskTimeoutSec = 1;

async function runBackgroundTask() {
  await new Promise(resolve => {
    const fiveMinutesInMs = 5 * 60 * 1000;
    setTimeout(resolve, fiveMinutesInMs);
  });
  return EXIT_CODE.SUCCESS;
}
