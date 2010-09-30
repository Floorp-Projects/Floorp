// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests installing a signed add-on that has been tampered with after signing.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Tampered Signed XPI": TESTROOT + "signed-tampered.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should only be 1 item listed in the confirmation dialog");
  is(items[0].name, "Tampered Signed XPI", "Should have seen the name from the trigger list");
  is(items[0].url, TESTROOT + "signed-tampered.xpi", "Should have listed the correct url for the item");
  is(items[0].cert, "(Object Signer)", "Should have seen the signer");
  is(items[0].signed, "true", "Should have listed the item as signed");
  return true;
}

function check_xpi_install(addon, status) {
  is(status, -260, "Install should fail");
}

function finish_test() {
  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
