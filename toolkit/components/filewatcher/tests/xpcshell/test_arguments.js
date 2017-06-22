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
 * Test for addPath usage with null arguments.
 */
add_task(async function test_null_args_addPath() {

  let watcher = makeWatcher();
  let testPath = "someInvalidPath";

  // Define a dummy callback function. In this test no callback is
  // expected to be called.
  let dummyFunc = function(changed) {
    do_throw("Not expected in this test.");
  };

  // Check for error when passing a null first argument
  try {
    watcher.addPath(testPath, null, dummyFunc);
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NULL_POINTER)
      throw ex;
    do_print("Initialisation thrown NS_ERROR_NULL_POINTER as expected.");
  }

  // Check for error when passing both null arguments
  try {
    watcher.addPath(testPath, null, null);
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NULL_POINTER)
      throw ex;
    do_print("Initialisation thrown NS_ERROR_NULL_POINTER as expected.");
  }
});

/**
 * Test for removePath usage with null arguments.
 */
add_task(async function test_null_args_removePath() {

  let watcher = makeWatcher();
  let testPath = "someInvalidPath";

  // Define a dummy callback function. In this test no callback is
  // expected to be called.
  let dummyFunc = function(changed) {
    do_throw("Not expected in this test.");
  };

  // Check for error when passing a null first argument
  try {
    watcher.removePath(testPath, null, dummyFunc);
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NULL_POINTER)
      throw ex;
    do_print("Initialisation thrown NS_ERROR_NULL_POINTER as expected.");
  }

  // Check for error when passing both null arguments
  try {
    watcher.removePath(testPath, null, null);
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NULL_POINTER)
      throw ex;
    do_print("Initialisation thrown NS_ERROR_NULL_POINTER as expected.");
  }
});
