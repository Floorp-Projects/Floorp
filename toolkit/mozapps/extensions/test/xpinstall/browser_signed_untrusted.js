// ----------------------------------------------------------------------------
// Tests installing an add-on signed by an untrusted certificate through an
// InstallTrigger call in web content.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Untrusted Signed XPI": TESTROOT + "signed-untrusted.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  var items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should only be 1 item listed in the confirmation dialog");
  is(items[0].name, "Signed XPI Test", "Should have had the filename for the item name");
  is(items[0].url, TESTROOT + "signed-untrusted.xpi", "Should have listed the correct url for the item");
  is(items[0].icon, "", "Should have listed no icon for the item");
  is(items[0].signed, "false", "Should have listed the item as unsigned");
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
