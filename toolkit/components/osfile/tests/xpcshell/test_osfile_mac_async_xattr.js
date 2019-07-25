/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Tests to ensure that extended attribute (xattr) related operations function
 * correctly.
 *
 * These tests are MacOS specific.
 */

/**
 * Test that happy path functionality works.
 */
add_task(async function test_mac_xattr() {
  // Create a file to manipulate
  let path = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "test_osfile_mac_xattr.tmp"
  );
  await OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let expected_size = 2;
    let xattr_value = new Uint8Array(expected_size);
    // We need a separate array storing the expected value as xattr_value will
    // be transferred and become invalid after we set the xattr.
    let expected_value = new Uint8Array(expected_size);
    for (let i = 0; i < expected_size; i++) {
      let num = Math.floor(Math.random() * 0xff);
      xattr_value[i] = num;
      expected_value[i] = num;
    }

    // Set attribute on file.
    await OS.File.macSetXAttr(path, "user.foo", xattr_value);

    // Make sure attribute is set and getting it works.
    let xattr_readback = await OS.File.macGetXAttr(path, "user.foo");
    Assert.equal(xattr_readback.length, expected_size);
    for (let i = 0; i < expected_size; i++) {
      Assert.equal(xattr_readback[i], expected_value[i]);
    }

    // Remove the attribute and verify it is no longer set.
    await OS.File.macRemoveXAttr(path, "user.foo");
    let got_error = false;
    try {
      // Should throw since the attribute is no longer set.
      await OS.File.macGetXAttr(path, "user.foo");
    } catch (e) {
      Assert.ok(e instanceof OS.File.Error);
      Assert.ok(e.toString().includes("getxattr"));
      got_error = true;
    }
    Assert.ok(got_error);
  } finally {
    await removeTestFile(path);
  }
});

add_task(async function test_mac_xattr_missing_file() {
  let path = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "test_osfile_mac_xattr_missing_file.tmp"
  );
  // Intentionally don't create a file, we want to make sure ops fail.
  let exists = await OS.File.exists(path);
  Assert.equal(false, exists);

  // Setting on non-existent file fails.
  let got_first_error = false;
  try {
    let xattr_value = new Uint8Array(1);
    await OS.File.macSetXAttr(path, "user.foo", xattr_value);
  } catch (e) {
    Assert.ok(e instanceof OS.File.Error);
    Assert.ok(e.toString().includes("setxattr"));
    got_first_error = true;
  }
  Assert.ok(got_first_error);

  // Getting xattr from non-existent file fails.
  let got_second_error = false;
  try {
    await OS.File.macGetXAttr(path, "user.foo");
  } catch (e) {
    Assert.ok(e instanceof OS.File.Error);
    Assert.ok(e.toString().includes("getxattr"));
    got_second_error = true;
  }
  Assert.ok(got_second_error);

  // Removing xattr from non-existent file fails.
  let got_third_error = false;
  try {
    await OS.File.macRemoveXAttr(path, "user.foo");
  } catch (e) {
    Assert.ok(e instanceof OS.File.Error);
    Assert.ok(e.toString().includes("removexattr"));
    got_third_error = true;
  }
  Assert.ok(got_third_error);
});
