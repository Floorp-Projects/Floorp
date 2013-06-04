/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");

let cs = Cc["@mozilla.org/consoleservice;1"].
  getService(Ci.nsIConsoleService);
let ps = Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefService);

function makeBuffer(length) {
  let string = "x";
  while (string.length < length) {
    string = string + string;
  }
  if (string.length > length) {
    string = string.substring(length - string.length);
  }
  return string;
}

/**
 * @resolves |true| if execution proceeded without warning,
 * |false| if there was a warning.
 */
function checkWarning(pref, buffer) {
  let deferred = Promise.defer();
  let complete = false;
  let listener = {
    observe: function(event) {
      let message = event.message;
      if (!(message.startsWith("Warning: attempting to write")
            && message.contains(pref))) {
        return;
      }
      if (complete) {
        return;
      }
      complete = true;
      do_print("Warning while setting " + pref);
      cs.unregisterListener(listener);
      deferred.resolve(true);
    }
  };
  do_timeout(1000, function() {
    if (complete) {
      return;
    }
    complete = true;
    do_print("No warning while setting " + pref);
    cs.unregisterListener(listener);
    deferred.resolve(false);
  });
  cs.registerListener(listener);
  ps.setCharPref(pref, buffer);
  return deferred.promise;
}

function run_test() {
  run_next_test();
}

add_task(function() {
  // Very large change, should fail
  try {
    ps.setCharPref("string.fail", makeBuffer(16 * 1024 * 1024));
    do_print("Writing to string.fail should have failed");
    do_check_true(false); // We should have failed
  } catch (ex if ex.result == Cr.NS_ERROR_OUT_OF_MEMORY) {
    do_print("Writing to string.fail failed for the right reasons");
    do_check_true(true); // Failed for the right reason
  } catch (ex) {
    do_print("Writing to string.fail failed for bad reasons");
    do_check_true(false); // Failed for the wrong reason
  }

  // Simple change, shouldn't cause a warning
  do_print("Checking that a simple change doesn't cause a warning");
  let buf = makeBuffer(100);
  let warned = yield checkWarning("string.accept", buf);
  do_check_false(warned);

  // Large change, should cause a warning
  do_print("Checking that a large change causes a warning");
  buf = makeBuffer(32 * 1024);
  warned = yield checkWarning("string.warn", buf);
  do_check_true(warned);
});
