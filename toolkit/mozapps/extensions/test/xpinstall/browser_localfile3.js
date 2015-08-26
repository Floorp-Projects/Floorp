// ----------------------------------------------------------------------------
// Tests installing an add-on from a local file with whitelisting disabled.
// This should be blocked by the whitelist check.
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  // Disable direct request whitelisting, installing from file should be blocked.
  Services.prefs.setBoolPref("xpinstall.whitelist.directRequest", false);

  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);

  var chromeroot = extractChromeRoot(gTestPath);
  try {
    var xpipath = cr.convertChromeURL(makeURI(chromeroot + "unsigned.xpi")).spec;
  } catch (ex) {
    var xpipath = chromeroot + "unsigned.xpi"; //scenario where we are running from a .jar and already extracted
  }

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    gBrowser.loadURI(xpipath);
  });
}

function allow_blocked(installInfo) {
  ok(true, "Seen blocked");
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");

  Services.prefs.clearUserPref("xpinstall.whitelist.directRequest");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
