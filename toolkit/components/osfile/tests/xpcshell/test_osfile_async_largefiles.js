/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/ctypes.jsm");
Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

/**
 * A test to check that .getPosition/.setPosition work with large files.
 * (see bug 952997)
 */

// Test setPosition/getPosition.
function test_setPosition(forward, current, backward) {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_largefiles.tmp");

  // Clear any left-over files from previous runs.
  try {
    yield OS.File.remove(path);
  } catch (ex if ex.becauseNoSuchFile) {
    // ignore
  }

  try {
    let file = yield OS.File.open(path, {write:true, append:false});
    try {
      let pos = 0;

      // 1. seek forward from start
      do_print("Moving forward: " + forward);
      yield file.setPosition(forward, OS.File.POS_START);
      pos += forward;
      do_check_eq((yield file.getPosition()), pos);

      // 2. seek forward from current position
      do_print("Moving current: " + current);
      yield file.setPosition(current, OS.File.POS_CURRENT);
      pos += current;
      do_check_eq((yield file.getPosition()), pos);

      // 3. seek backward from current position
      do_print("Moving current backward: " + backward);
      yield file.setPosition(-backward, OS.File.POS_CURRENT);
      pos -= backward;
      do_check_eq((yield file.getPosition()), pos);

    } finally {
      yield file.setPosition(0, OS.File.POS_START);
      yield file.close();
    }
  } catch(ex) {
    try {
      yield OS.File.remove(path);
    } catch (ex if ex.becauseNoSuchFile) {
      // ignore.
    }
    do_throw(ex);
  }
}

// Test setPosition/getPosition expected failures.
function test_setPosition_failures() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_largefiles.tmp");

  // Clear any left-over files from previous runs.
  try {
    yield OS.File.remove(path);
  } catch (ex if ex.becauseNoSuchFile) {
    // ignore
  }

  try {
    let file = yield OS.File.open(path, {write:true, append:false});
    try {
      let pos = 0;

      // 1. Use an invalid position value
      try {
        yield file.setPosition(0.5, OS.File.POS_START);
        do_throw("Shouldn't have succeeded");
      } catch (ex) {
        do_check_true(ex.toString().contains("expected type"));
      }
      // Since setPosition should have bailed, it shouldn't have moved the
      // file pointer at all.
      do_check_eq((yield file.getPosition()), 0);

      // 2. Use an invalid position value
      try {
        yield file.setPosition(0xffffffff + 0.5, OS.File.POS_START);
        do_throw("Shouldn't have succeeded");
      } catch (ex) {
        do_check_true(ex.toString().contains("expected type"));
      }
      // Since setPosition should have bailed, it shouldn't have moved the
      // file pointer at all.
      do_check_eq((yield file.getPosition()), 0);

      // 3. Use a position that cannot be represented as a double
      try {
        // Not all numbers after 9007199254740992 can be represented as a
        // double. E.g. in js 9007199254740992 + 1 == 9007199254740992
        yield file.setPosition(9007199254740992, OS.File.POS_START);
        yield file.setPosition(1, OS.File.POS_CURRENT);
        do_throw("Shouldn't have succeeded");
      } catch (ex) {
        do_print(ex.toString());
        do_check_true(!!ex.message);
      }

    } finally {
      yield file.setPosition(0, OS.File.POS_START);
      yield file.close();
      try {
        yield OS.File.remove(path);
      } catch (ex if ex.becauseNoSuchFile) {
        // ignore.
      }
    }
  } catch(ex) {
    do_throw(ex);
  }
}

function run_test() {
  // First verify stuff works for small values.
  add_task(test_setPosition.bind(null, 0, 100, 50));
  add_task(test_setPosition.bind(null, 1000, 100, 50));
  add_task(test_setPosition.bind(null, 1000, -100, -50));

  if (OS.Constants.Win || ctypes.off_t.size >= 8) {
    // Now verify stuff still works for large values.
    // 1. Multiple small seeks, which add up to > MAXINT32
    add_task(test_setPosition.bind(null, 0x7fffffff, 0x7fffffff, 0));
    // 2. Plain large seek, that should end up at 0 again.
    // 0xffffffff also happens to be the INVALID_SET_FILE_POINTER value on
    // Windows, so this also tests the error handling
    add_task(test_setPosition.bind(null, 0, 0xffffffff, 0xffffffff));
    // 3. Multiple large seeks that should end up > MAXINT32.
    add_task(test_setPosition.bind(null, 0xffffffff, 0xffffffff, 0xffffffff));
    // 5. Multiple large seeks with negative offsets.
    add_task(test_setPosition.bind(null, 0xffffffff, -0x7fffffff, 0x7fffffff));

    // 6. Check failures
    add_task(test_setPosition_failures);
  }

  run_next_test();
}
