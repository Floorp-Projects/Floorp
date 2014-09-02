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
 * Test the watcher correctly handles two watches sharing the same
 * change callback.
 */
add_task(function* test_watch_with_shared_callback() {

  // Create and watch two sub-directories of the profile directory so we don't
  // catch notifications we're not interested in (i.e. "startupCache").
  let watchedDirs =
    [
      OS.Path.join(OS.Constants.Path.profileDir, "filewatcher_playground"),
      OS.Path.join(OS.Constants.Path.profileDir, "filewatcher_playground2")
    ];

  yield OS.File.makeDir(watchedDirs[0]);
  yield OS.File.makeDir(watchedDirs[1]);

  let tempFileName = "test_filecreation.tmp";

  // Instantiate and initialize the native watcher.
  let watcher = makeWatcher();
  let deferred = Promise.defer();

  // Watch both directories using the same callbacks.
  yield promiseAddPath(watcher, watchedDirs[0], deferred.resolve, deferred.reject);
  yield promiseAddPath(watcher, watchedDirs[1], deferred.resolve, deferred.reject);

  // Remove the watch for the first directory, but keep watching
  // for changes in the second: we need to make sure the callback
  // survives the removal of the first watch.
  watcher.removePath(watchedDirs[0], deferred.resolve, deferred.reject);

  // Create a file within the watched directory.
  let tmpFilePath = OS.Path.join(watchedDirs[1], tempFileName);
  yield OS.File.writeAtomic(tmpFilePath, "some data");

  // Wait until the watcher informs us that the file was created.
  let changed = yield deferred.promise;
  do_check_eq(changed, tmpFilePath);

  // Remove the watch and free the associated memory (we need to
  // reuse 'deferred.resolve' and 'deferred.reject' to unregister).
  watcher.removePath(watchedDirs[1], deferred.resolve, deferred.reject);

  // Remove the test directories and all of their content.
  yield OS.File.removeDir(watchedDirs[0]);
  yield OS.File.removeDir(watchedDirs[1]);
});
