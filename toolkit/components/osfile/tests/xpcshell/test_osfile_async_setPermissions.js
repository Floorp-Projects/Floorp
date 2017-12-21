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
 * numeric value is 0o777 or lower, it is padded on the left with zeroes to
 * four digits wide.
 * Sample outputs:  0022, 0644, 04755.
 */
function format_mode(mode) {
  if (mode <= 0o777) {
    return ("0000" + mode.toString(8)).slice(-4);
  }
    return "0" + mode.toString(8);

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

// Sequence of setPermission parameters and expected file mode.  The first test
// checks the permissions when the file is first created.
var testSequence = [
  [null,                                        apply_umask(0o600)],
  [{ unixMode: 0o4777 },                        apply_umask(0o4777)],
  [{ unixMode: 0o4777, unixHonorUmask: false }, 0o4777],
  [{ unixMode: 0o4777, unixHonorUmask: true },  apply_umask(0o4777)],
  [undefined,                                   apply_umask(0o600)],
  [{ unixMode: 0o666 },                         apply_umask(0o666)],
  [{ unixMode: 0o600 },                         apply_umask(0o600)],
  [{ unixMode: 0 },                             0],
  [{},                                          apply_umask(0o600)],
];

// Test application to paths.
add_task(async function test_path_setPermissions() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_setPermissions_path.tmp");
  await OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    for (let [options, expectedMode] of testSequence) {
      if (options !== null) {
        do_print("Setting permissions to " + JSON.stringify(options));
        await OS.File.setPermissions(path, options);
      }

      let stat = await OS.File.stat(path);
      Assert.equal(format_mode(stat.unixMode), format_mode(expectedMode));
    }
  } finally {
    await OS.File.remove(path);
  }
});

// Test application to open files.
add_task(async function test_file_setPermissions() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setPermissions_file.tmp");
  await OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let fd = await OS.File.open(path, { write: true });
    try {
      for (let [options, expectedMode] of testSequence) {
        if (options !== null) {
          do_print("Setting permissions to " + JSON.stringify(options));
          await fd.setPermissions(options);
        }

        let stat = await fd.stat();
        Assert.equal(format_mode(stat.unixMode), format_mode(expectedMode));
      }
    } finally {
      await fd.close();
    }
  } finally {
    await OS.File.remove(path);
  }
});
