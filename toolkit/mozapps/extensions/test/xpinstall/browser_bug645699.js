// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an InstallTrigger call in web
// content. This should be blocked by the whitelist check.
// This verifies bug 645699
function test() {
  if (
    !SpecialPowers.Services.prefs.getBoolPref(
      "extensions.InstallTrigger.enabled"
    ) ||
    !SpecialPowers.Services.prefs.getBoolPref(
      "extensions.InstallTriggerImpl.enabled"
    )
  ) {
    ok(true, "InstallTrigger is not enabled");
    return;
  }

  // prompt prior to download
  SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.postDownloadThirdPartyPrompt", false],
      ["extensions.InstallTrigger.requireUserInput", false],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
    ],
  });

  Harness.installConfirmCallback = confirm_install;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
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
