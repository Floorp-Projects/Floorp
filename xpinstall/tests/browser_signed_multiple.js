// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests installing two signed add-ons in the same trigger works.
// This verifies bug 453545
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Signed XPI": TESTROOT + "signed.xpi",
    "Signed XPI 2": TESTROOT + "signed2.xpi",
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  items = window.document.getElementById("itemList").childNodes;
  is(items.length, 2, "Should be 2 items listed in the confirmation dialog");
  is(items[0].name, "Signed XPI", "Should have seen the name from the trigger list");
  is(items[0].url, TESTROOT + "signed.xpi", "Should have listed the correct url for the item");
  is(items[0].cert, "(Object Signer)", "Should have seen the signer");
  is(items[0].signed, "true", "Should have listed the item as signed");
  is(items[1].name, "Signed XPI 2", "Should have seen the name from the trigger list");
  is(items[1].url, TESTROOT + "signed2.xpi", "Should have listed the correct url for the item");
  is(items[1].cert, "(Object Signer)", "Should have seen the signer");
  is(items[1].signed, "true", "Should have listed the item as signed");
  return true;
}

function check_xpi_install(addon, status) {
  is(status, 0, "Installs should succeed");
}

function finish_test() {
  var em = Components.classes["@mozilla.org/extensions/manager;1"]
                     .getService(Components.interfaces.nsIExtensionManager);
  em.cancelInstallItem("signed-xpi@tests.mozilla.org");
  em.cancelInstallItem("signed-xpi2@tests.mozilla.org");

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
