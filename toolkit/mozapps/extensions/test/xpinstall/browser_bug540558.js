// ----------------------------------------------------------------------------
// Tests that calling InstallTrigger.installChrome works
function test() {
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(gBrowser, TESTROOT + "bug540558.html");
}

function check_xpi_install(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
