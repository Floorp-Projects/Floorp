
"use strict";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/identity/LogUtils.jsm');

function toggle_debug() {
  do_test_pending();

  function Wrapper() {
    Services.prefs.addObserver('toolkit.identity.debug', this, false);
  }
  Wrapper.prototype = {
    QueryInterface: XPCOMUtils.generateQI([ci.nsISupports, Ci.nsIObserver]),

    observe: function observe(aSubject, aTopic, aData) {
      if (aTopic === "nsPref:changed") {
        // race condition?
dump("WOOT! it changed");
        do_check_eq(Logger._debug, true);
        do_test_finished();
        run_next_test();
      }
    }
  };

  var wrapper = new Wrapper();
  Services.prefs.setBoolPref('toolkit.identity.debug', true);
}

// test that things don't break

function logAlias(...args) {
  Logger.log.call(["log alias"].concat(args));
}
function reportErrorAlias(...args) {
  Loggerl.reportError.call(["report error alias"].concat(args));
}

function test_log() {
  Logger.log("log test", "I like pie");
  do_test_finished();
  run_next_test();
}

function test_reportError() {
  Logger.reportError("log test", "We are out of pies!!!");
  do_test_finished();
  run_next_test();
}

function test_wrappers() {
  logAlias("I like potatoes");
  do_test_finished();
  reportErrorAlias("Too much red bull");
}

let TESTS = [
    toggle_debug,
    test_log,
    test_reportError,
    test_wrappers
];

function run_test() {
  run_next_test();
}