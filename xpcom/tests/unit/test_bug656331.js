Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

function info(s) {
  dump("TEST-INFO | test_bug656331.js | " + s + "\n");
}

var gMessageExpected = /Native module.*has version 3.*expected/;
var gFound = false;

const kConsoleListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleListener]),
  
  observe: function listener_observe(message) {
    if (gMessageExpected.test(message.message))
      gFound = true;
  }
};

function run_test() {
  let cs = Components.classes["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService);
  cs.registerListener(kConsoleListener);

  let manifest = do_get_file('components/bug656331.manifest');
  registerAppManifest(manifest);

  do_check_false("{f18fb09b-28b4-4435-bc5b-8027f18df743}" in Components.classesByID);

  do_test_pending();
  Components.classes["@mozilla.org/thread-manager;1"].
    getService(Ci.nsIThreadManager).mainThread.dispatch(function() {
      cs.unregisterListener(kConsoleListener);
      do_check_true(gFound);
      do_test_finished();
    }, 0);
}
