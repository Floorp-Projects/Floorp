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
 * Test the watcher correctly notifies of a file creation in a subdirectory
 * of the watched sub-directory (recursion).
 */
add_task(async function test_watch_recursively() {
  // Create and watch a sub-directory of the profile directory so we don't
  // catch notifications we're not interested in (i.e. "startupCache").
  let watchedDir = OS.Path.join(
    OS.Constants.Path.profileDir,
    "filewatcher_playground"
  );
  await OS.File.makeDir(watchedDir);

  // We need at least 2 levels of directories to test recursion.
  let subdirectory = OS.Path.join(watchedDir, "level1");
  await OS.File.makeDir(subdirectory);

  let tempFileName = "test_filecreation.tmp";

  // Instantiate and initialize the native watcher.
  let watcher = makeWatcher();
  let deferred = PromiseUtils.defer();

  let tmpFilePath = OS.Path.join(subdirectory, tempFileName);

  // Add the profile directory to the watch list and wait for the file watcher
  // to start watching it.
  await promiseAddPath(watcher, watchedDir, deferred.resolve, deferred.reject);

  // Create a file within the subdirectory of the watched directory.
  await OS.File.writeAtomic(tmpFilePath, "some data");

  // Wait until the watcher informs us that the file was created.
  let changed = await deferred.promise;
  Assert.equal(changed, tmpFilePath);

  // Remove the watch and free the associated memory (we need to
  // reuse 'deferred.resolve' and 'deferred.reject' to unregister).
  await promiseRemovePath(
    watcher,
    watchedDir,
    deferred.resolve,
    deferred.reject
  );

  // Remove the test directory and all of its content.
  await OS.File.removeDir(watchedDir);
});
