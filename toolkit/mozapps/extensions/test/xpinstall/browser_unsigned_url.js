// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on by navigating directly to the url
function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv(
    {
      set: [
        // Relax the user input requirements while running this test.
        ["xpinstall.userActivation.required", false],
      ],
    },
    runTest
  );
}

function runTest() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    BrowserTestUtils.startLoadingURIString(gBrowser, TESTROOT + "unsigned.xpi");
  });
}

function confirm_install(panel) {
  is(panel.getAttribute("name"), "XPI Test", "Should have seen the name");
  return true;
}

function install_ended(install, addon) {
  return addon.uninstall();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
