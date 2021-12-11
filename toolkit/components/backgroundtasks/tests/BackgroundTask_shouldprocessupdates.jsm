/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

async function runBackgroundTask(commandLine) {
  var env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  let exitCode =
    env.get("MOZ_TEST_PROCESS_UPDATES") == "!ShouldProcessUpdates()" ? 80 : 81;
  console.debug(`runBackgroundTask: shouldprocessupdates`, {
    exists: env.exists("MOZ_TEST_PROCESS_UPDATES"),
    get: env.get("MOZ_TEST_PROCESS_UPDATES"),
  });
  console.error(
    `runBackgroundTask: shouldprocessupdates exiting with exitCode ${exitCode}`
  );

  return exitCode;
}
