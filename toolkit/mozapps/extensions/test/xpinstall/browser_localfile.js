// ----------------------------------------------------------------------------
// Tests installing an local file works when loading the url
function test() {
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );

  var chromeroot = extractChromeRoot(gTestPath);
  var xpipath = chromeroot + "unsigned.xpi";
  try {
    xpipath = cr.convertChromeURL(makeURI(chromeroot + "amosigned.xpi")).spec;
  } catch (ex) {
    // scenario where we are running from a .jar and already extracted
  }

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    BrowserTestUtils.startLoadingURIString(gBrowser, xpipath);
  });
}

function install_ended(install, addon) {
  Assert.deepEqual(
    install.installTelemetryInfo,
    { source: "file-url" },
    "Got the expected install.installTelemetryInfo"
  );

  return addon.uninstall();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
