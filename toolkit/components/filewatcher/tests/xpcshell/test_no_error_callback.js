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
 * Test the component behaves correctly when no error callback is
 * provided and an error occurs.
 */
add_task(async function test_error_with_no_error_callback() {

  let watcher = makeWatcher();
  let testPath = "someInvalidPath";

  // Define a dummy callback function. In this test no callback is
  // expected to be called.
  let dummyFunc = function(changed) {
    do_throw("Not expected in this test.");
  };

  // We don't pass an error callback and try to watch an invalid
  // path.
  watcher.addPath(testPath, dummyFunc);
});

/**
 * Test the component behaves correctly when no error callback is
 * provided (no error should occur).
 */
add_task(async function test_watch_single_path_file_creation_no_error_cb() {

  // Create and watch a sub-directory of the profile directory so we don't
  // catch notifications we're not interested in (i.e. "startupCache").
  let watchedDir = OS.Path.join(OS.Constants.Path.profileDir, "filewatcher_playground");
  await OS.File.makeDir(watchedDir);

  let tempFileName = "test_filecreation.tmp";

  // Instantiate and initialize the native watcher.
  let watcher = makeWatcher();
  let deferred = Promise.defer();

  // Watch the profile directory but do not pass an error callback.
  await promiseAddPath(watcher, watchedDir, deferred.resolve);

  // Create a file within the watched directory.
  let tmpFilePath = OS.Path.join(watchedDir, tempFileName);
  await OS.File.writeAtomic(tmpFilePath, "some data");

  // Wait until the watcher informs us that the file was created.
  let changed = await deferred.promise;
  Assert.equal(changed, tmpFilePath);

  // Remove the watch and free the associated memory (we need to
  // reuse 'deferred.resolve' to unregister).
  watcher.removePath(watchedDir, deferred.resolve);

  // Remove the test directory and all of its content.
  await OS.File.removeDir(watchedDir);
});
