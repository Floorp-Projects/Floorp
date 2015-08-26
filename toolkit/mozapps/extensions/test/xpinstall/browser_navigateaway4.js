// ----------------------------------------------------------------------------
// Tests that navigating to a new origin cancels ongoing installs and closes
// the install UI.
let sawUnload = null;

function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  sawUnload = BrowserTestUtils.waitForEvent(window, "unload");

  gBrowser.loadURI(TESTROOT2 + "enabled.html");

  return Harness.leaveOpen;
}

function install_ended(install, addon) {
  ok(false, "Should not have seen installs complete");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been successfully installed");

  Services.perms.remove("http://example.com", "install");

  sawUnload.then(() => {
    ok(true, "The install UI should have closed itself.");
    gBrowser.removeCurrentTab();
    Harness.finish();
  });
}
