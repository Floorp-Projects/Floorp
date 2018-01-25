Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/* global registerAppManifest */

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
  Services.console.registerListener(kConsoleListener);

  let manifest = do_get_file("components/bug656331.manifest");
  registerAppManifest(manifest);

  Assert.equal(false, "{f18fb09b-28b4-4435-bc5b-8027f18df743}" in Components.classesByID);

  do_test_pending();
  Services.tm.dispatchToMainThread(function() {
    Services.console.unregisterListener(kConsoleListener);
    Assert.ok(gFound);
    do_test_finished();
  });
}
