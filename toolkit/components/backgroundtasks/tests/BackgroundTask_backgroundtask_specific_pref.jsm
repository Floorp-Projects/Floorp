/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

async function runBackgroundTask(commandLine) {
  let pref = commandLine.length
    ? commandLine.getArgument(0)
    : "test.backgroundtask_specific_pref.exitCode";

  // 0, 1, 2, 3 are all meaningful exit codes already.
  let exitCode = Services.prefs.getIntPref(pref, 4);

  console.error(
    `runBackgroundTask: backgroundtask_specific_pref read pref '${pref}', exiting with exitCode ${exitCode}`
  );

  return exitCode;
}
