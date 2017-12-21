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
 * Tests that the watcher correctly notifies of a directory creation when watching
 * a single path.
 */
add_task(async function test_watch_single_path_directory_creation() {

  // Create and watch a sub-directory of the profile directory so we don't
  // catch notifications we're not interested in (i.e. "startupCache").
  let watchedDir = OS.Path.join(OS.Constants.Path.profileDir, "filewatcher_playground");
  await OS.File.makeDir(watchedDir);

  let tmpDirPath = OS.Path.join(watchedDir, "testdir");

  // Instantiate and initialize the native watcher.
  let watcher = makeWatcher();
  let deferred = Promise.defer();

  // Add the profile directory to the watch list and wait for the file watcher
  // to start watching.
  await promiseAddPath(watcher, watchedDir, deferred.resolve, deferred.reject);

  // Once ready, create a directory within the watched directory.
  await OS.File.makeDir(tmpDirPath);

  // Wait until the watcher informs us that the file has changed.
  let changed = await deferred.promise;
  Assert.equal(changed, tmpDirPath);

  // Remove the watch and free the associated memory (we need to
  // reuse 'deferred.resolve' and 'deferred.reject' to unregister).
  await promiseRemovePath(watcher, watchedDir, deferred.resolve, deferred.reject);

  // Clean up the test directory.
  await OS.File.removeDir(watchedDir);
});
