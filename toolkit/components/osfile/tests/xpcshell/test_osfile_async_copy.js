"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * A file that we know exists and that can be used for reading.
 */
var EXISTING_FILE = "test_osfile_async_copy.js";

/**
 * Fetch asynchronously the contents of a file using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} path The _absolute_ path to the file.
 * @return {promise}
 * @resolves {string} The contents of the file.
 */
var reference_fetch_file = function reference_fetch_file(path) {
  return new Promise((resolve, reject) => {
    let file = new FileUtils.File(path);
    NetUtil.asyncFetch({
      uri: NetUtil.newURI(file),
      loadUsingSystemPrincipal: true
    }, function(stream, status) {
        if (!Components.isSuccessCode(status)) {
          reject(status);
          return;
        }
        let result, reject;
        try {
          result = NetUtil.readInputStreamToString(stream, stream.available());
        } catch (x) {
          reject = x;
        }
        stream.close();
        if (reject) {
          reject(reject);
        } else {
          resolve(result);
        }
      });

  });
};

/**
 * Compare asynchronously the contents two files using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} a The _absolute_ path to the first file.
 * @param {string} b The _absolute_ path to the second file.
 *
 * @resolves {null}
 */
var reference_compare_files = async function reference_compare_files(a, b) {
  let a_contents = await reference_fetch_file(a);
  let b_contents = await reference_fetch_file(b);
  // Not using do_check_eq to avoid dumping the whole file to the log.
  // It is OK to === compare here, as both variables contain a string.
  do_check_true(a_contents === b_contents);
};

/**
 * Test to ensure that OS.File.copy works.
 */
async function test_copymove(options = {}) {
  let source = OS.Path.join((await OS.File.getCurrentDirectory()),
                            EXISTING_FILE);
  let dest = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_copy_dest.tmp");
  let dest2 = OS.Path.join(OS.Constants.Path.tmpDir,
                           "test_osfile_async_copy_dest2.tmp");
  try {
    // 1. Test copy.
    await OS.File.copy(source, dest, options);
    await reference_compare_files(source, dest);
    // 2. Test subsequent move.
    await OS.File.move(dest, dest2);
    await reference_compare_files(source, dest2);
    // 3. Check that the moved file was really moved.
    do_check_eq((await OS.File.exists(dest)), false);
  } finally {
    await removeTestFile(dest);
    await removeTestFile(dest2);
  }
}

// Regular copy test.
add_task(test_copymove);
// Userland copy test.
add_task(test_copymove.bind(null, {unixUserland: true}));

add_task(do_test_finished);
