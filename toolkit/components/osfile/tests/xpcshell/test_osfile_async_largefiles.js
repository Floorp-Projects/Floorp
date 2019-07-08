/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ctypes } = ChromeUtils.import("resource://gre/modules/ctypes.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

/**
 * A test to check that .getPosition/.setPosition work with large files.
 * (see bug 952997)
 */

// Test setPosition/getPosition.
async function test_setPosition(forward, current, backward) {
  let path = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "test_osfile_async_largefiles.tmp"
  );

  // Clear any left-over files from previous runs.
  await removeTestFile(path);

  try {
    let file = await OS.File.open(path, { write: true, append: false });
    try {
      let pos = 0;

      // 1. seek forward from start
      info("Moving forward: " + forward);
      await file.setPosition(forward, OS.File.POS_START);
      pos += forward;
      Assert.equal(await file.getPosition(), pos);

      // 2. seek forward from current position
      info("Moving current: " + current);
      await file.setPosition(current, OS.File.POS_CURRENT);
      pos += current;
      Assert.equal(await file.getPosition(), pos);

      // 3. seek backward from current position
      info("Moving current backward: " + backward);
      await file.setPosition(-backward, OS.File.POS_CURRENT);
      pos -= backward;
      Assert.equal(await file.getPosition(), pos);
    } finally {
      await file.setPosition(0, OS.File.POS_START);
      await file.close();
    }
  } catch (ex) {
    await removeTestFile(path);
  }
}

// Test setPosition/getPosition expected failures.
async function test_setPosition_failures() {
  let path = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "test_osfile_async_largefiles.tmp"
  );

  // Clear any left-over files from previous runs.
  await removeTestFile(path);

  try {
    let file = await OS.File.open(path, { write: true, append: false });
    try {
      // 1. Use an invalid position value
      try {
        await file.setPosition(0.5, OS.File.POS_START);
        do_throw("Shouldn't have succeeded");
      } catch (ex) {
        Assert.ok(ex.toString().includes("can't pass"));
      }
      // Since setPosition should have bailed, it shouldn't have moved the
      // file pointer at all.
      Assert.equal(await file.getPosition(), 0);

      // 2. Use an invalid position value
      try {
        await file.setPosition(0xffffffff + 0.5, OS.File.POS_START);
        do_throw("Shouldn't have succeeded");
      } catch (ex) {
        Assert.ok(ex.toString().includes("can't pass"));
      }
      // Since setPosition should have bailed, it shouldn't have moved the
      // file pointer at all.
      Assert.equal(await file.getPosition(), 0);

      // 3. Use a position that cannot be represented as a double
      try {
        // Not all numbers after 9007199254740992 can be represented as a
        // double. E.g. in js 9007199254740992 + 1 == 9007199254740992
        await file.setPosition(9007199254740992, OS.File.POS_START);
        await file.setPosition(1, OS.File.POS_CURRENT);
        do_throw("Shouldn't have succeeded");
      } catch (ex) {
        info(ex.toString());
        Assert.ok(!!ex);
      }
    } finally {
      await file.setPosition(0, OS.File.POS_START);
      await file.close();
      await removeTestFile(path);
    }
  } catch (ex) {
    do_throw(ex);
  }
}

function run_test() {
  // First verify stuff works for small values.
  add_task(test_setPosition.bind(null, 0, 100, 50));
  add_task(test_setPosition.bind(null, 1000, 100, 50));
  add_task(test_setPosition.bind(null, 1000, -100, -50));

  if (OS.Constants.Win || ctypes.off_t.size >= 8) {
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
