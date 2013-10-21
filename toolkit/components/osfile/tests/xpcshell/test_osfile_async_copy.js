"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Promise.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * A file that we know exists and that can be used for reading.
 */
let EXISTING_FILE = "test_osfile_async_copy.js";

/**
 * Fetch asynchronously the contents of a file using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} path The _absolute_ path to the file.
 * @return {promise}
 * @resolves {string} The contents of the file.
 */
let reference_fetch_file = function reference_fetch_file(path) {
  let promise = Promise.defer();
  let file = new FileUtils.File(path);
  NetUtil.asyncFetch(file,
    function(stream, status) {
      if (!Components.isSuccessCode(status)) {
        promise.reject(status);
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
        promise.reject(reject);
      } else {
        promise.resolve(result);
      }
  });
  return promise.promise;
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
let reference_compare_files = function reference_compare_files(a, b) {
  let a_contents = yield reference_fetch_file(a);
  let b_contents = yield reference_fetch_file(b);
  // Not using do_check_eq to avoid dumping the whole file to the log.
  // It is OK to === compare here, as both variables contain a string.
  do_check_true(a_contents === b_contents);
};

/**
 * Test to ensure that OS.File.copy works.
 */
function test_copymove(options = {}) {
  let source = OS.Path.join((yield OS.File.getCurrentDirectory()),
                            EXISTING_FILE);
  let dest = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_copy_dest.tmp");
  let dest2 = OS.Path.join(OS.Constants.Path.tmpDir,
                           "test_osfile_async_copy_dest2.tmp");
  try {
    // 1. Test copy.
    yield OS.File.copy(source, dest, options);
    yield reference_compare_files(source, dest);
    // 2. Test subsequent move.
    yield OS.File.move(dest, dest2);
    yield reference_compare_files(source, dest2);
    // 3. Check that the moved file was really moved.
    do_check_eq((yield OS.File.exists(dest)), false);
  } finally {
    try {
      yield OS.File.remove(dest);
    } catch (ex if ex.becauseNoSuchFile) {
      // ignore
    }
    try {
      yield OS.File.remove(dest2);
    } catch (ex if ex.becauseNoSuchFile) {
      // ignore
    }
  }
}

// Regular copy test.
add_task(test_copymove);
// Userland copy test.
add_task(test_copymove.bind(null, {unixUserland: true}));

add_task(do_test_finished);
