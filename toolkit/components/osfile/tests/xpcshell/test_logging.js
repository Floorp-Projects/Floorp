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

  do_print("Test starting");

  // Create a console listener.
  let consoleListener = {
    observe(aMessage) {
      // Ignore unexpected messages.
      if (!(aMessage instanceof Components.interfaces.nsIConsoleMessage)) {
        return;
      }
      // This is required, as printing to the |Services.console|
      // while in the observe function causes an exception.
      do_execute_soon(function() {
        do_print("Observing message " + aMessage.message);
        if (aMessage.message.indexOf("TEST OS") < 0) {
          return;
        }

        ++messageCount;
        if (messageCount == 1) {
          Assert.equal(aMessage.message, "TEST OS {\"name\":\"test\"}\n");
        }
        if (messageCount == 2) {
          Assert.equal(aMessage.message, "TEST OS name is test\n");
          toggleConsoleListener(false);
          do_test_finished();
        }
      });
    }
  };

  // Set/Unset the console listener.
  function toggleConsoleListener(pref) {
    do_print("Setting console listener: " + pref);
    Services.prefs.setBoolPref("toolkit.osfile.log", pref);
    Services.prefs.setBoolPref("toolkit.osfile.log.redirect", pref);
    Services.console[pref ? "registerListener" : "unregisterListener"](
      consoleListener);
  }

  toggleConsoleListener(true);

  let objectDefault = {name: "test"};
  let CustomToString = function() {
    this.name = "test";
  };
  CustomToString.prototype.toString = function() {
    return "name is " + this.name;
  };
  let objectCustom = new CustomToString();

  do_print(OS.Shared.LOG.toSource());

  do_print("Logging 1");
  OS.Shared.LOG(objectDefault);

  do_print("Logging 2");
  OS.Shared.LOG(objectCustom);
  // Once both messages are observed OS.Shared.DEBUG, and OS.Shared.TEST
  // are reset to false.
}
