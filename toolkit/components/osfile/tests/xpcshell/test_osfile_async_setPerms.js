/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * A test to ensure that OS.File.setPermissions and
 * OS.File.prototype.setPermissions are all working correctly.
 * (see bug 1001849)
 * These functions are currently Unix-specific.  The manifest skips
 * the test on Windows.
 */

/**
 * Helper function for test logging: prints a POSIX file permission mode as an
 * octal number, with a leading '0' per C (not JS) convention.  When the
 * numeric value is 0777 or lower, it is padded on the left with zeroes to
 * four digits wide.
 * Sample outputs:  0022, 0644, 04755.
 */
function format_mode(mode) {
  if (mode <= 0o777) {
    return ("0000" + mode.toString(8)).slice(-4);
  } else {
    return "0" + mode.toString(8);
  }
}

/**
 * Use this function to compare two mode values; it prints both values as
 * octal numbers in the log.
 */
function do_check_modes_eq(left, right, text) {
  text = text + ": " + format_mode(left) + " === " + format_mode(right);
  do_report_result(left === right, text, Components.stack.caller, false);
}

const _umask = OS.Constants.Sys.umask;
do_print("umask: " + format_mode(_umask));

/**
 * Compute the mode that a file should have after applying the umask,
 * whatever it happens to be.
 */
function apply_umask(mode) {
  return mode & ~_umask;
}

// Test application to paths.
add_task(function*() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_setPerms_nonproto.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let stat;

    yield OS.File.setPermissions(path, {unixMode: 0o4777});
    stat = yield OS.File.stat(path);
    do_check_modes_eq(stat.unixMode, 0o4777,
                      "setPermissions(path, 04777)");

    yield OS.File.setPermissions(path, {unixMode: 0o4777,
                                        unixHonorUmask: true});
    stat = yield OS.File.stat(path);
    do_check_modes_eq(stat.unixMode, apply_umask(0o4777),
                      "setPermissions(path, 04777&~umask)");

    yield OS.File.setPermissions(path);
    stat = yield OS.File.stat(path);
    do_check_modes_eq(stat.unixMode, apply_umask(0o666),
                      "setPermissions(path, {})");

    yield OS.File.setPermissions(path, {unixMode: 0});
    stat = yield OS.File.stat(path);
    do_check_modes_eq(stat.unixMode, 0,
                      "setPermissions(path, 0000)");

  } finally {
    yield OS.File.remove(path);
  }
});

// Test application to open files.
add_task(function*() {
  // First, create a file we can mess with.
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_proto.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let fd = yield OS.File.open(path, {write: true});
    let stat;

    yield fd.setPermissions({unixMode: 0o4777});
    stat = yield fd.stat();
    do_check_modes_eq(stat.unixMode, 0o4777,
                      "fd.setPermissions(04777)");

    yield fd.setPermissions({unixMode: 0o4777, unixHonorUmask: true});
    stat = yield fd.stat();
    do_check_modes_eq(stat.unixMode, apply_umask(0o4777),
                      "fd.setPermissions(04777&~umask)");

    yield fd.setPermissions();
    stat = yield fd.stat();
    do_check_modes_eq(stat.unixMode, apply_umask(0o666),
                      "fd.setPermissions({})");

    yield fd.setPermissions({unixMode: 0});
    stat = yield fd.stat();
    do_check_modes_eq(stat.unixMode, 0,
                      "fd.setPermissions(0000)");

    yield fd.close();
  } finally {
    yield OS.File.remove(path);
  }
});

function run_test() {
  run_next_test();
}
