// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Test whether an install fails when authentication is required and bad
// credentials are given
// This verifies bug 312473
function test() {
  Harness.authenticationCallback = get_auth_info;
  Harness.installEndedCallback = check_xpi_install;
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

function check_xpi_install(addon, status) {
  is(status, -228, "Install should fail");
}

function finish_test() {
  var authMgr = Components.classes['@mozilla.org/network/http-auth-manager;1']
                          .getService(Components.interfaces.nsIHttpAuthManager);
  authMgr.clearAll();

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
