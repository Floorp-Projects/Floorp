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
 * Tests the watcher by watching several resources.
 * This test creates the specified number of directory inside the profile
 * directory, adds each one of them to the watched list the creates
 * a file in them in order to trigger the notification.
 * The test keeps track of the number of times the changes callback is
 * called in order to verify the success of the test.
 */
add_task(async function test_watch_multi_paths() {

  // The number of resources to watch. We expect changes for
  // creating a file within each directory.
  let resourcesToWatch = 5;
  let watchedDir = OS.Constants.Path.profileDir;

  // The directories to be watched will be created with.
  let tempDirNameBase = "FileWatcher_Test_";
  let tempFileName = "test.tmp";

  // Instantiate the native watcher.
  let watcher = makeWatcher();

  // This will be used to keep track of the number of changes within the watched
  // resources.
  let detectedChanges = 0;
  let watchedResources = 0;
  let unwatchedResources = 0;

  let deferredChanges = Promise.defer();
  let deferredSuccesses = Promise.defer();
  let deferredShutdown = Promise.defer();

  // Define the change callback function.
  let changeCallback = function(changed) {
      info(changed + " has changed.");

      detectedChanges += 1;

      // Resolve the promise if we get all the expected changes.
      if (detectedChanges === resourcesToWatch) {
        deferredChanges.resolve();
      }
    };

  // Define the watch success callback function.
  let watchSuccessCallback = function(resourcePath) {
      info(resourcePath + " is being watched.");

      watchedResources += 1;

      // Resolve the promise when all the resources are being
      // watched.
      if (watchedResources === resourcesToWatch) {
        deferredSuccesses.resolve();
      }
    };

  // Define the watch success callback function.
  let unwatchSuccessCallback = function(resourcePath) {
      info(resourcePath + " is being un-watched.");

      unwatchedResources += 1;

      // Resolve the promise when all the resources are being
      // watched.
      if (unwatchedResources === resourcesToWatch) {
        deferredShutdown.resolve();
      }
    };

  // Create the directories and add them to the watched resources list.
  for (let i = 0; i < resourcesToWatch; i++) {
    let tmpSubDirPath = OS.Path.join(watchedDir, tempDirNameBase + i);
    info("Creating the " + tmpSubDirPath + " directory.");
    await OS.File.makeDir(tmpSubDirPath);
    watcher.addPath(tmpSubDirPath, changeCallback, deferredChanges.reject, watchSuccessCallback);
  }

  // Wait until the watcher informs us that all the desired resources
  // are being watched.
  await deferredSuccesses.promise;

  // Create a file within each watched directory.
  for (let i = 0; i < resourcesToWatch; i++) {
    let tmpFilePath = OS.Path.join(watchedDir, tempDirNameBase + i, tempFileName);
    await OS.File.writeAtomic(tmpFilePath, "test content");
  }

  // Wait until the watcher informs us that all the files were created.
  await deferredChanges.promise;

  // Remove the directories we have created.
  for (let i = 0; i < resourcesToWatch; i++) {
    let tmpSubDirPath = OS.Path.join(watchedDir, tempDirNameBase + i);
    watcher.removePath(tmpSubDirPath, changeCallback, deferredChanges.reject, unwatchSuccessCallback);
  }

  // Wait until the watcher un-watches the resources.
  await deferredShutdown.promise;
});
