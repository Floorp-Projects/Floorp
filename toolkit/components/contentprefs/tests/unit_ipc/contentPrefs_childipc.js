
function run_test() {
  do_check_true(inChildProcess(), "test harness should never call us directly");

  var cps = Cc["@mozilla.org/content-pref/service;1"].
            createInstance(Ci.nsIContentPrefService);

  // Cannot get general values
  try {
    cps.getPref("group", "name")
    do_check_false(true, "Must have thrown exception on getting general value");
  }
  catch(e) { }

  // Cannot set general values
  try {
    cps.setPref("group", "name", "someValue2");
    do_check_false(true, "Must have thrown exception on setting general value");
  }
  catch(e) { }

  // Can set&get whitelisted values
  cps.setPref("group", "browser.upload.lastDir", "childValue");
  do_check_eq(cps.getPref("group", "browser.upload.lastDir"), "childValue");

  // Test sending URI
  var ioSvc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
  var uri = ioSvc.newURI("http://mozilla.org", null, null);
  cps.setPref(uri, "browser.upload.lastDir", "childValue2");
  do_check_eq(cps.getPref(uri, "browser.upload.lastDir"), "childValue2");

  // Previous value
  do_check_eq(cps.getPref("group", "browser.upload.lastDir"), "childValue");

  // Tell parent to finish and clean up
  cps.wrappedJSObject.messageManager.sendSyncMessage('ContentPref:QUIT');
}

