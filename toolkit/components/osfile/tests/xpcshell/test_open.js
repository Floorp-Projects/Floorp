/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  run_next_test();
}

/**
 * Test OS.File.open for reading:
 * - with an existing file (should succeed);
 * - with a non-existing file (should fail);
 * - with inconsistent arguments (should fail).
 */
add_task(async function() {
  // Attempt to open a file that does not exist, ensure that it yields the
  // appropriate error.
  try {
    let fd = await OS.File.open(OS.Path.join(".", "This file does not exist"));
    do_check_true(false, "File opening 1 succeeded (it should fail)");
  } catch (err if err instanceof OS.File.Error && err.becauseNoSuchFile) {
    do_print("File opening 1 failed " + err);
  }

  // Attempt to open a file with the wrong args, so that it fails before
  // serialization, ensure that it yields the appropriate error.
  do_print("Attempting to open a file with wrong arguments");
  try {
    let fd = await OS.File.open(1, 2, 3);
    do_check_true(false, "File opening 2 succeeded (it should fail)" + fd);
  } catch (err) {
    do_print("File opening 2 failed " + err);
    do_check_false(err instanceof OS.File.Error,
                   "File opening 2 returned something that is not a file error");
    do_check_true(err.constructor.name == "TypeError",
                  "File opening 2 returned a TypeError");
  }

  // Attempt to open a file correctly
  do_print("Attempting to open a file correctly");
  let openedFile = await OS.File.open(OS.Path.join(do_get_cwd().path, "test_open.js"));
  do_print("File opened correctly");

  do_print("Attempting to close a file correctly");
  await openedFile.close();

  do_print("Attempting to close a file again");
  await openedFile.close();
});

/**
 * Test the error thrown by OS.File.open when attempting to open a directory
 * that does not exist.
 */
add_task(async function test_error_attributes () {

  let dir = OS.Path.join(do_get_profile().path, "test_osfileErrorAttrs");
  let fpath = OS.Path.join(dir, "test_error_attributes.txt");

  try {
    await OS.File.open(fpath, {truncate: true}, {});
    do_check_true(false, "Opening path suceeded (it should fail) " + fpath);
  } catch (err) {
    do_check_true(err instanceof OS.File.Error);
    do_check_true(err.becauseNoSuchFile);
  }
});
