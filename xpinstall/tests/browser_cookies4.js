// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Test that an install that requires cookies to be sent fails when cookies
// are set and third party cookies are disabled and the request is to a third
// party.
// This verifies bug 462739
function test() {
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var cm = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager2);
  cm.add("example.com", "/browser/xpinstall/tests", "xpinstall", "true", false,
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

function check_xpi_install(addon, status) {
  is(status, -228, "Install should fail");
}

function finish_test() {
  var cm = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager2);
  cm.remove("example.com", "xpinstall", "/browser/xpinstall/tests", false);

  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
