// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through a navigation. Should be
// blocked since the referer is not whitelisted even though the target is.
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT2 + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "unsigned.xpi", makeURI(TESTROOT2 + "test.html"));
}

function allow_blocked(installInfo) {
  is(installInfo.originatingWindow, gBrowser.contentWindow, "Install should have been triggered by the right window");
  is(installInfo.originatingURI.spec, TESTROOT2 + "test.html", "Install should have been triggered by the right uri");
  return false;
}

function finish_test() {
  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
