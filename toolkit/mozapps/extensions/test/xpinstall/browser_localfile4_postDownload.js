// ----------------------------------------------------------------------------
// Tests installing an add-on from a local file with file origins disabled.
// This should be blocked by the origin allowed check.
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  // Prevent the Harness from ending the test on download cancel.
  Harness.downloadCancelledCallback = () => {
    return false;
  };
  Harness.setup();

  // Disable local file install, installing by file referrer should be blocked.
  Services.prefs.setBoolPref("xpinstall.whitelist.fileRequest", false);

  var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );

  var chromeroot = extractChromeRoot(gTestPath);
  var xpipath = chromeroot;
  try {
    xpipath = cr.convertChromeURL(makeURI(chromeroot)).spec;
  } catch (ex) {
    // scenario where we are running from a .jar and already extracted
  }
  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    xpipath + "installtrigger.html?" + triggers
  );
}

function allow_blocked(installInfo) {
  ok(true, "Seen blocked");
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");

  Services.prefs.clearUserPref("xpinstall.whitelist.fileRequest");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
