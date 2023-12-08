/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

export async function runBackgroundTask(commandLine) {
  let pid = Services.appinfo.processID;
  let finalProcessDelaySec = 10;
  let waitTimeoutSec = 30;
  let s = "";
  for (let i = 0; i < commandLine.length; i++) {
    if (i > 0) {
      s += " ";
    }
    s += "'" + commandLine.getArgument(i) + "'";
  }
  console.log(`runBackgroundTask: automaticrestart ${pid}: '${s}'`);
  let updateProcessor = Cc[
    "@mozilla.org/updates/update-processor;1"
  ].createInstance(Ci.nsIUpdateProcessor);
  // When this background task is first called by test_backgroundtask_automaticrestart.js,
  // it will enter the else-if "no-wait" block and launch a new background task. That
  // task then re-enters this code in the if "restart-pid" block, and writes to the file.
  // This file is then read by the original test file to verify that the new background
  // task was launched and completed successfully.
  if (commandLine.findFlag("restart-pid", false) != -1) {
    await IOUtils.writeUTF8(commandLine.getArgument(1), `written from ${pid}`, {
      mode: "overwrite",
    });
    // This timeout is meant to activate the waitForProcessExit consistently.
    // There shouldn't be any race conditions here as we always wait for
    // this process to exit before reading the file, but this makes what's
    // happening a bit more apparent and prevents cases where this process
    // can exit before we hit the waitForProcessExit call. This isn't an
    // error case but it defeats the purpose of the test.
    return new Promise(resolve =>
      setTimeout(resolve, finalProcessDelaySec * 1000)
    );
  } else if (commandLine.getArgument(0) == "-no-wait") {
    let newPid =
      updateProcessor.attemptAutomaticApplicationRestartWithLaunchArgs([
        "-test-only-automatic-restart-no-wait",
      ]);
    console.log(
      `runBackgroundTask: spawned automatic restart task from ${pid} with pid ${newPid}`
    );
    updateProcessor.waitForProcessExit(newPid, waitTimeoutSec * 1000);
  }
  return 0;
}
