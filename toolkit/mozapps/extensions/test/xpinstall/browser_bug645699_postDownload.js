// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an InstallTrigger call in web
// content. This should be blocked by the origin allow check.
// This verifies bug 645699
function test() {
  // This test depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.installConfirmCallback = confirm_install;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  // Prevent the Harness from ending the test on download cancel.
  Harness.downloadCancelledCallback = () => {
    return false;
  };

  Harness.setup();

  PermissionTestUtils.add(
    "http://example.org/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser, TESTROOT + "bug645699.html");
}

function allow_blocked(installInfo) {
  is(
    installInfo.browser,
    gBrowser.selectedBrowser,
    "Install should have been triggered by the right browser"
  );
  is(
    installInfo.originatingURI.spec,
    gBrowser.currentURI.spec,
    "Install should have been triggered by the right uri"
  );
  return false;
}

function confirm_install(panel) {
  ok(false, "Should not see the install dialog");
  return false;
}

function finish_test(count) {
  is(count, 0, "0 Add-ons should have been successfully installed");
  PermissionTestUtils.remove("http://example.org/", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
