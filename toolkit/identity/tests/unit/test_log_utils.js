
"use strict";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');

XPCOMUtils.defineLazyGetter(this, "logger", function() {
  Cu.import('resource://gre/modules/identity/LogUtils.jsm');
  return getLogger("Identity test", "toolkit.identity.debug");
});

function toggle_debug() {
  do_test_pending();

  function Wrapper() {
    this.init();
  }
  Wrapper.prototype = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

    observe: function observe(aSubject, aTopic, aData) {
      if (aTopic === "nsPref:changed") {
        // race condition?
        do_check_eq(logger._enabled, true);
        do_test_finished();
        run_next_test();
      }
    },

    init: function() {
      Services.prefs.addObserver('toolkit.identity.debug', this, false);
    }
  };

  var wrapper = new Wrapper();
  Services.prefs.setBoolPref('toolkit.identity.debug', true);
}

// test that things don't break

function test_log() {
  logger.log("log test", "I like pie");
  do_test_finished();
  run_next_test();
}

function test_warning() {
  logger.warning("similar log test", "We are still out of pies!!!");
  do_test_finished();
  run_next_test();
}

function test_error() {
  logger.error("My head a splode");
  do_test_finished();
  run_next_test();
}


let TESTS = [
// XXX fix me 
//    toggle_debug,
    test_log,
    test_warning,
    test_error
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}

