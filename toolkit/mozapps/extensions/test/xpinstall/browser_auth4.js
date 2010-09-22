// ----------------------------------------------------------------------------
// Test whether a request for auth for an XPI switches to the appropriate tab
function test() {
  Harness.authenticationCallback = get_auth_info;
  Harness.downloadFailedCallback = download_failed;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "authRedirect.sjs?" + TESTROOT + "unsigned.xpi"
  }));
  gNewTab = gBrowser.addTab();
  gBrowser.getBrowserForTab(gNewTab).loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function get_auth_info() {
  is(gBrowser.selectedTab, gNewTab, "Should have focused the tab loading the XPI");
  return [ "testuser", "testpass" ];
}

function download_failed(install) {
  ok(false, "Install should not have failed");
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  var authMgr = Components.classes['@mozilla.org/network/http-auth-manager;1']
                          .getService(Components.interfaces.nsIHttpAuthManager);
  authMgr.clearAll();

  Services.perms.remove("example.com", "install");

  gBrowser.removeTab(gNewTab);
  Harness.finish();
}
