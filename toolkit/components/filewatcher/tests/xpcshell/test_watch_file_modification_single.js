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
 * Tests that the watcher correctly notifies of a file modification when watching
 * a single path.
 */
add_task(async function test_watch_single_path_file_modification() {
  // Create and watch a sub-directory of the profile directory so we don't
  // catch notifications we're not interested in (i.e. "startupCache").
  let watchedDir = OS.Path.join(
    OS.Constants.Path.profileDir,
    "filewatcher_playground"
  );
  await OS.File.makeDir(watchedDir);

  let tempFileName = "test_filemodification.tmp";

  // Instantiate and initialize the native watcher.
  let watcher = makeWatcher();
  let deferred = PromiseUtils.defer();

  // Create a file within the directory to be watched. We do this
  // before watching the directory so we do not get the creation notification.
  let tmpFilePath = OS.Path.join(watchedDir, tempFileName);
  await OS.File.writeAtomic(tmpFilePath, "some data");

  // Add the profile directory to the watch list and wait for the file watcher
  // to start watching it.
  await promiseAddPath(watcher, watchedDir, deferred.resolve, deferred.reject);

  // Once ready, modify the file to trigger the notification.
  await OS.File.writeAtomic(tmpFilePath, "some new data");

  // Wait until the watcher informs us that the file has changed.
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
