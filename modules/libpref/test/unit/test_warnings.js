/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Promise.jsm");

let cs = Cc["@mozilla.org/consoleservice;1"].
  getService(Ci.nsIConsoleService);
let ps = Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefService);

function makeBuffer(length) {
  return new Array(length + 1).join('x');
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
