/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A task that exercises various functionality to witness contents of
// the temporary profile created during background tasks.  This will
// be a dumping ground for functionality that writes to the profile.

"use strict";

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

async function runBackgroundTask(commandLine) {
  console.error("runBackgroundTask: is_profile_slim");

  // For now, just verify contents of profile after a network request.
  if (commandLine.length != 1) {
    console.error("Single URL argument required");
    return 1;
  }

  let response = await fetch(commandLine.getArgument(0));
  console.error(`response status code: ${response.status}`);

  return response.ok ? EXIT_CODE.SUCCESS : 11;
}
