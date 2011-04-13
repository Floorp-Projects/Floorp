/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

 /**
  * Test that preventive maintenance outputs to the console.
  */

// Include PlacesDBUtils module.
Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

function run_test() {
  do_test_pending();

  // Ensure a database exists.
  PlacesUtils.history;

  let consoleListener = {
    observe: function(aMsg) {
      do_log_info("Got console message:\n" + aMsg.message);
      do_check_eq(aMsg.message.split("\n")[0], "[ Places Maintenance ]");
      Services.console.unregisterListener(this);
      do_test_finished();
    },
 
    QueryInterface: XPCOMUtils.generateQI([
     Ci.nsIConsoleListener
    ]),
  };
  Services.console.reset();
  Services.console.registerListener(consoleListener);
  PlacesDBUtils.reindex();
}
