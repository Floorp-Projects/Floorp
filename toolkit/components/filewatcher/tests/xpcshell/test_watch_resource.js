/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function run_test() {
  // Set up profile. We will use profile path create some test files
  do_get_profile();

  // Start executing the tests
  run_next_test();
}

/**
 * Test watching non-existing path
 */
add_task(function* test_watching_non_existing() {
  let notExistingDir =
    OS.Path.join(OS.Constants.Path.profileDir, "absolutelyNotExisting");

  // Instantiate the native watcher.
  let watcher = makeWatcher();
  let deferred = Promise.defer();

  // Try watch a path which doesn't exist.
  watcher.addPath(notExistingDir, deferred.reject, deferred.resolve);

  // Wait until the watcher informs us that there was an error.
  let error = yield deferred.promise;
  do_check_eq(error, Components.results.NS_ERROR_FILE_NOT_FOUND);
});
