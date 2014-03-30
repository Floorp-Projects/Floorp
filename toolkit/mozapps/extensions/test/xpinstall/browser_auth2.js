// ----------------------------------------------------------------------------
// Test whether an install fails when authentication is required and bad
// credentials are given
// This verifies bug 312473
function test() {
  requestLongerTimeout(2);
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
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function get_auth_info() {
  return [ "baduser", "badpass" ];
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_NETWORK_FAILURE, "Install should have failed");
}

function install_ended(install, addon) {
  ok(false, "Add-on should not have installed");
  install.cancel();
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  var authMgr = Components.classes['@mozilla.org/network/http-auth-manager;1']
                          .getService(Components.interfaces.nsIHttpAuthManager);
  authMgr.clearAll();

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
