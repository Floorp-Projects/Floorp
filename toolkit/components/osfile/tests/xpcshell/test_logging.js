"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

/**
 * Tests logging by passing OS.Shared.LOG both an object with its own
 * toString method, and one with the default.
 */
function run_test() {
  do_test_pending();
  let messageCount = 0;

  // Create a console listener.
  let consoleListener = {
    observe: function (aMessage) {
      ++messageCount;
      //Ignore unexpected messages.
      if (!(aMessage instanceof Components.interfaces.nsIConsoleMessage)) {
        return;
      }
      if (aMessage.message.indexOf("TEST OS") < 0) {
        return;
      }

      if(messageCount == 1) {
       do_check_eq(aMessage.message, "TEST OS {\"name\":\"test\"}\n");
      }
      if(messageCount == 2) {
        do_check_eq(aMessage.message, "TEST OS name is test\n");
        // This is required, as printing to the |Services.console|
        // while in the observe function causes an exception.
        do_execute_soon(function() {
          toggleConsoleListener(false);
          do_test_finished();
        });
      }
    }
  };

  // Set/Unset the console listener.
  function toggleConsoleListener (pref) {
    OS.Shared.DEBUG = pref;
    OS.Shared.TEST = pref;
    Services.console[pref ? "registerListener" : "unregisterListener"](
      consoleListener);
  }

  toggleConsoleListener(true);

  let objectDefault = {name: "test"};
  let CustomToString = function() {
    this.name = "test";
  }
  CustomToString.prototype.toString = function() {
    return "name is " + this.name;
  }
  let objectCustom = new CustomToString();
  OS.Shared.LOG(objectDefault);
  OS.Shared.LOG(objectCustom);
  // Once both messages are observed OS.Shared.DEBUG, and OS.Shared.TEST
  // are reset to false.
}

