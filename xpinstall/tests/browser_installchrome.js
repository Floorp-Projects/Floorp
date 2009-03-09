// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/browser/xpinstall/tests/harness.js", this);

// ----------------------------------------------------------------------------
// Tests that calling InstallTrigger.installChrome works
function test() {
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                     .getService(Components.interfaces.nsIPermissionManager);
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installchrome.html? " + encodeURIComponent(TESTROOT + "unsigned.xpi"));
}

function check_xpi_install(addon, status) {
  is(status, 0, "Install should succeed");
}

function finish_test() {
  var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                     .getService(Components.interfaces.nsIPermissionManager);
  pm.remove("example.com", "install");

  var em = Components.classes["@mozilla.org/extensions/manager;1"]
                     .getService(Components.interfaces.nsIExtensionManager);
  em.cancelInstallItem("unsigned-xpi@tests.mozilla.org");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
