/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test with sessionCheckpoints.json containing invalid data
 */
add_task(function test_invalid_file() {
  // Write bogus data to checkpoint file
  let data = "1234";
  yield OS.File.writeAtomic(sessionCheckpointsPath, data,
                            {tmpPath: sessionCheckpointsPath + ".tmp"});

  // An invalid file will cause |init| to return null
  let status = yield CrashMonitor.init();
  do_check_true(status === null ? true : false);

  // and |previousCheckpoints| will be null
  let checkpoints = yield CrashMonitor.previousCheckpoints;
  do_check_true(checkpoints === null ? true : false);
});
