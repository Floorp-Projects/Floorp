/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test with non-existing sessionCheckpoints.json
 */
add_task(async function test_missing_file() {
  CrashMonitor.init();
  let checkpoints = await CrashMonitor.previousCheckpoints;
  Assert.equal(checkpoints, null);
});
