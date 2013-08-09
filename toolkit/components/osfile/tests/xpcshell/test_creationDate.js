"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * Test to ensure that deprecation warning is issued on use
 * of creationDate.
 */
add_task(function test_deprecatedCreationDate () {
  let deferred = Promise.defer();
  let consoleListener = {
    observe: function (aMessage) {
      if(aMessage.message.indexOf("Field 'creationDate' is deprecated.") > -1) {
        do_print("Deprecation message printed");
        do_check_true(true);
        Services.console.unregisterListener(consoleListener);
        deferred.resolve();
      }
    }
  };
  let currentDir = yield OS.File.getCurrentDirectory();
  let path = OS.Path.join(currentDir, "test_creationDate.js");

  Services.console.registerListener(consoleListener);
  (yield OS.File.stat(path)).creationDate;
});

add_task(do_test_finished);
