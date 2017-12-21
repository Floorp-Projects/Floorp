"use strict";

info("starting tests");

Components.utils.import("resource://gre/modules/osfile.jsm");

/**
 * A test to check that the |append| mode flag is correctly implemented.
 * (see bug 925865)
 */

function setup_mode(mode) {
  // Complete mode.
  let realMode = {
    read: true,
    write: true
  };
  for (let k in mode) {
    realMode[k] = mode[k];
  }
  return realMode;
}

// Test append mode.
async function test_append(mode) {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_append.tmp");

  // Clear any left-over files from previous runs.
  await removeTestFile(path);

  try {
    mode = setup_mode(mode);
    mode.append = true;
    if (mode.trunc) {
      // Pre-fill file with some data to see if |trunc| actually works.
      await OS.File.writeAtomic(path, new Uint8Array(500));
    }
    let file = await OS.File.open(path, mode);
    try {
      await file.write(new Uint8Array(1000));
      await file.setPosition(0, OS.File.POS_START);
      await file.read(100);
      // Should be at offset 100, length 1000 now.
      await file.write(new Uint8Array(100));
      // Should be at offset 1100, length 1100 now.
      let stat = await file.stat();
      Assert.equal(1100, stat.size);
    } finally {
      await file.close();
    }
  } catch (ex) {
    await removeTestFile(path);
  }
}

// Test no-append mode.
async function test_no_append(mode) {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_noappend.tmp");

  // Clear any left-over files from previous runs.
  await removeTestFile(path);

  try {
    mode = setup_mode(mode);
    mode.append = false;
    if (mode.trunc) {
      // Pre-fill file with some data to see if |trunc| actually works.
      await OS.File.writeAtomic(path, new Uint8Array(500));
    }
    let file = await OS.File.open(path, mode);
    try {
      await file.write(new Uint8Array(1000));
      await file.setPosition(0, OS.File.POS_START);
      await file.read(100);
      // Should be at offset 100, length 1000 now.
      await file.write(new Uint8Array(100));
      // Should be at offset 200, length 1000 now.
      let stat = await file.stat();
      Assert.equal(1000, stat.size);
    } finally {
      await file.close();
    }
  } finally {
    await removeTestFile(path);
  }
}

var test_flags = [
  {},
  {create: true},
  {trunc: true}
];
function run_test() {
  do_test_pending();

  for (let t of test_flags) {
    add_task(test_append.bind(null, t));
    add_task(test_no_append.bind(null, t));
  }
  add_task(do_test_finished);

  run_next_test();
}
