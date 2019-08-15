// ----------------------------------------------------------------------------
// Tests that the InstallTrigger callback can redirect to a relative url.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(gBrowser, TESTROOT + "triggerredirect.html");
}

function confirm_install(panel) {
  is(panel.getAttribute("name"), "XPI Test", "Should have seen the name");
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  PermissionTestUtils.remove("http://example.com", "install");

  is(
    gBrowser.currentURI.spec,
    TESTROOT + "triggerredirect.html#foo",
    "Should have redirected"
  );

  gBrowser.removeCurrentTab();
  Harness.finish();
}
