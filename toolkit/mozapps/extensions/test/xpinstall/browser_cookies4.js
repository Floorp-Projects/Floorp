// ----------------------------------------------------------------------------
// Test that an install that requires cookies to be sent fails when cookies
// are set and third party cookies are disabled and the request is to a third
// party.
// This verifies bug 462739
function test() {
  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var cm = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager2);
  cm.add("example.org", "/browser/" + RELATIVE_DIR, "xpinstall", "true", false,
         false, true, (Date.now() / 1000) + 60);

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);

  var triggers = encodeURIComponent(JSON.stringify({
    "Cookie check": TESTROOT2 + "cookieRedirect.sjs?" + TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_NETWORK_FAILURE, "Install should fail");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  var cm = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager2);
  cm.remove("example.org", "xpinstall", "/browser/" + RELATIVE_DIR, false);

  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
