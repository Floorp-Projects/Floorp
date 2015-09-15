/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * A test to ensure that OS.File.setPermissions and
 * OS.File.prototype.setPermissions are all working correctly.
 * (see bug 1022816)
 * The manifest tests on Windows.
 */

// Sequence of setPermission parameters.
var testSequence = [
  [ { winAttributes: { readOnly: true, system: true, hidden: true } },
    { readOnly: true, system: true, hidden: true } ],
  [ { winAttributes: { readOnly: false } },
    { readOnly: false, system: true, hidden: true } ],
  [ { winAttributes: { system: false } },
    { readOnly: false, system: false, hidden: true } ],
  [ { winAttributes: { hidden: false } },
    { readOnly: false, system: false, hidden: false } ],
  [ { winAttributes: {readOnly: true, system: false, hidden: false} },
    { readOnly: true, system: false, hidden: false } ],
  [ { winAttributes: {readOnly: false, system: true, hidden: false} },
    { readOnly: false, system: true, hidden: false } ],
  [ { winAttributes: {readOnly: false, system: false, hidden: true} },
    { readOnly: false, system: false, hidden: true } ],
];

// Test application to paths.
add_task(function* test_path_setPermissions() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_win_async_setPermissions_path.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    for (let [options, attributesExpected] of testSequence) {
      if (options !== null) {
        do_print("Setting permissions to " + JSON.stringify(options));
        yield OS.File.setPermissions(path, options);
      }

      let stat = yield OS.File.stat(path);
      do_print("Got stat winAttributes: " + JSON.stringify(stat.winAttributes));

      do_check_eq(stat.winAttributes.readOnly, attributesExpected.readOnly);
      do_check_eq(stat.winAttributes.system, attributesExpected.system);
      do_check_eq(stat.winAttributes.hidden, attributesExpected.hidden);

    }
  } finally {
    yield OS.File.remove(path);
  }
});

// Test application to open files.
add_task(function* test_file_setPermissions() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_win_async_setPermissions_file.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let fd = yield OS.File.open(path, { write: true });
    try {
      for (let [options, attributesExpected] of testSequence) {
        if (options !== null) {
          do_print("Setting permissions to " + JSON.stringify(options));
          yield fd.setPermissions(options);
        }

        let stat = yield fd.stat();
        do_print("Got stat winAttributes: " + JSON.stringify(stat.winAttributes));
        do_check_eq(stat.winAttributes.readOnly, attributesExpected.readOnly);
        do_check_eq(stat.winAttributes.system, attributesExpected.system);
        do_check_eq(stat.winAttributes.hidden, attributesExpected.hidden);
      }
    } finally {
      yield fd.close();
    }
  } finally {
    yield OS.File.remove(path);
  }
});

// Test application to Check setPermissions on a non-existant file path.
add_task(function* test_non_existant_file_path_setPermissions() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_win_async_setPermissions_path.tmp");
  Assert.rejects(OS.File.setPermissions(path, {winAttributes: {readOnly: true}}),
                 /The system cannot find the file specified/,
                 "setPermissions failed as expected on a non-existant file path");
});

// Test application to Check setPermissions on a invalid file handle.
add_task(function* test_closed_file_handle_setPermissions() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_win_async_setPermissions_path.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let fd = yield OS.File.open(path, { write: true });
    yield fd.close();
    Assert.rejects(fd.setPermissions(path, {winAttributes: {readOnly: true}}),
                   /The handle is invalid/,
                   "setPermissions failed as expected on a invalid file handle");
  } finally {
    yield OS.File.remove(path);
  }
});

function run_test() {
  run_next_test();
}
