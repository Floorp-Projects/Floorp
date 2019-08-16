// ----------------------------------------------------------------------------
// Tests that calling InstallTrigger.startSoftwareUpdate works
function test() {
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT +
      "startsoftwareupdate.html? " +
      encodeURIComponent(TESTROOT + "amosigned.xpi")
  );
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  PermissionTestUtils.remove("http://example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
