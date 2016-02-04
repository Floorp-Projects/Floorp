/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function run_test() {
  // Set up profile. We will use profile path create some test files.
  do_get_profile();

  // Start executing the tests.
  run_next_test();
}

/**
 * Test removing non watched path
 */
add_task(function* test_remove_not_watched() {
  let nonExistingDir =
    OS.Path.join(OS.Constants.Path.profileDir, "absolutelyNotExisting");

  // Instantiate the native watcher.
  let watcher = makeWatcher();

  // Try to un-watch a path which wasn't being watched.
  watcher.removePath(
    nonExistingDir,
    function(changed) {
      do_throw("No change is expected in this test.");
    },
    function(xpcomError, osError) {
      // When removing a resource which wasn't being watched, it should silently
      // ignore the request.
      do_throw("Unexpected exception: "
               + xpcomError + " (XPCOM) "
               + osError + " (OS Error)");
    }
  );
});
