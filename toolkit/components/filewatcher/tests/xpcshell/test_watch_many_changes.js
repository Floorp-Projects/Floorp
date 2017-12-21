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
 * Test that we correctly handle watching directories when hundreds of files
 * change simultaneously.
 */
add_task(async function test_fill_notification_buffer() {

  // Create and watch a sub-directory of the profile directory so we don't
  // catch notifications we're not interested in (i.e. "startupCache").
  let watchedDir = OS.Path.join(OS.Constants.Path.profileDir, "filewatcher_playground");
  await OS.File.makeDir(watchedDir);

  // The number of files to create.
  let numberOfFiles = 100;
  let fileNameBase = "testFile";

  // This will be used to keep track of the number of changes within the watched
  // directory.
  let detectedChanges = 0;

  // We expect at least the following notifications for each file:
  //  - File creation
  //  - File deletion
  let expectedChanges = numberOfFiles * 2;

  // Instantiate the native watcher.
  let watcher = makeWatcher();
  let deferred = Promise.defer();

  // Initialise the change callback.
  let changeCallback = function(changed) {
      info(changed + " has changed.");

      detectedChanges += 1;

      // Resolve the promise if we get all the expected changes.
      if (detectedChanges >= expectedChanges) {
        deferred.resolve();
      }
    };

  // Add the profile directory to the watch list and wait for the file watcher
  // to start watching it.
  await promiseAddPath(watcher, watchedDir, changeCallback, deferred.reject);

  // Create and then remove the files within the watched directory.
  for (let i = 0; i < numberOfFiles; i++) {
    let tmpFilePath = OS.Path.join(watchedDir, fileNameBase + i);
    await OS.File.writeAtomic(tmpFilePath, "test content");
    await OS.File.remove(tmpFilePath);
  }

  // Wait until the watcher informs us that all the files were
  // created, modified and removed.
  await deferred.promise;

  // Remove the watch and free the associated memory (we need to
  // reuse 'changeCallback' and 'errorCallback' to unregister).
  await promiseRemovePath(watcher, watchedDir, changeCallback, deferred.reject);
});
