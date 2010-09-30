// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on by navigating directly to the url
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "unsigned.xpi");
}

function confirm_install(window) {
  items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should only be 1 item listed in the confirmation dialog");
  is(items[0].name, "unsigned.xpi", "Should have had the filename for the item name");
  is(items[0].url, TESTROOT + "unsigned.xpi", "Should have listed the correct url for the item");
  is(items[0].icon, "", "Should have listed no icon for the item");
  is(items[0].signed, "false", "Should have listed the item as unsigned");
  return true;
}

function check_xpi_install(addon, status) {
  is(status, 0, "Install should succeed");
}

function finish_test() {
  var em = Components.classes["@mozilla.org/extensions/manager;1"]
                     .getService(Components.interfaces.nsIExtensionManager);
  em.cancelInstallItem("unsigned-xpi@tests.mozilla.org");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
