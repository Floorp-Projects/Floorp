/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let {utils: Cu, interfaces: Ci} = Components;

let {OS} = Cu.import("resource://gre/modules/osfile.jsm", {});
let {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
let {Promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
let {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let {NetUtil} = Cu.import("resource://gre/modules/NetUtil.jsm", {});

Services.prefs.setBoolPref("toolkit.osfile.log", true);

/**
 * As add_task, but execute the test both with native operations and
 * without.
 */
function add_test_pair(generator) {
  add_task(function*() {
    do_print("Executing test " + generator.name + " with native operations");
    Services.prefs.setBoolPref("toolkit.osfile.native", true);
    return Task.spawn(generator);
  });
  add_task(function*() {
    do_print("Executing test " + generator.name + " without native operations");
    Services.prefs.setBoolPref("toolkit.osfile.native", false);
    return Task.spawn(generator);
  });
}

/**
 * Fetch asynchronously the contents of a file using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} path The _absolute_ path to the file.
 * @return {promise}
 * @resolves {string} The contents of the file.
 */
function reference_fetch_file(path, test) {
  do_print("Fetching file " + path);
  let deferred = Promise.defer();
  let file = new FileUtils.File(path);
  NetUtil.asyncFetch(file,
    function(stream, status) {
      if (!Components.isSuccessCode(status)) {
        deferred.reject(status);
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
        deferred.reject(reject);
      } else {
        deferred.resolve(result);
      }
  });
  return deferred.promise;
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
function reference_compare_files(a, b, test) {
  return Task.spawn(function*() {
    do_print("Comparing files " + a + " and " + b);
    let a_contents = yield reference_fetch_file(a, test);
    let b_contents = yield reference_fetch_file(b, test);
    do_check_eq(a_contents, b_contents);
  });
};
