// ----------------------------------------------------------------------------
// Tests installing an add-on from a local file with whitelisting disabled.
// This should be blocked by the whitelist check.
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  // Disable direct request whitelisting, installing from file should be blocked.
  Services.prefs.setBoolPref("xpinstall.whitelist.directRequest", false);

  var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );

  var chromeroot = extractChromeRoot(gTestPath);
  var xpipath = chromeroot + "amosigned.xpi";
  try {
    xpipath = cr.convertChromeURL(makeURI(xpipath)).spec;
  } catch (ex) {
    // scenario where we are running from a .jar and already extracted
  }

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    BrowserTestUtils.loadURI(gBrowser, xpipath);
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
