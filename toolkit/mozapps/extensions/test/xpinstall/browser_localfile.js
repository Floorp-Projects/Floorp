// ----------------------------------------------------------------------------
// Tests installing an local file works when loading the url
function test() {
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);
  var path = cr.convertChromeURL(makeURI(CHROMEROOT + "unsigned.xpi")).spec;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(path);
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
