/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

  // An invalid file will cause |init| to throw an exception
  try {
    let status = yield CrashMonitor.init();
    do_check_true(false);
  } catch (ex) {
    do_check_true(true);
  }

  // and |previousCheckpoints| will be rejected
  try {
    let checkpoints = yield CrashMonitor.previousCheckpoints;
    do_check_true(false);
  } catch (ex) {
    do_check_true(true);
  }
});
