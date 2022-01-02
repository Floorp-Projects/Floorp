/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test with sessionCheckpoints.json containing invalid JSON data
 */
add_task(async function test_invalid_file() {
  // Write bogus data to checkpoint file
  let data = "[}";
  await OS.File.writeAtomic(sessionCheckpointsPath, data, {
    tmpPath: sessionCheckpointsPath + ".tmp",
  });

  CrashMonitor.init();
  let checkpoints = await CrashMonitor.previousCheckpoints;
  Assert.equal(checkpoints, null);
});
