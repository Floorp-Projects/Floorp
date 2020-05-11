// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an InstallTrigger call in web
// content. This should be blocked by the whitelist check.
// This verifies bug 252830
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
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
  return true;
}

function confirm_install(panel) {
  is(panel.getAttribute("name"), "XPI Test", "Should have seen the name");
  return true;
}

function install_ended(install, addon) {
  return addon.uninstall();
}

const finish_test = async function(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  const results = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return {
        return: content.document.getElementById("return").textContent,
        status: content.document.getElementById("status").textContent,
      };
    }
  );

  is(results.return, "false", "installTrigger should seen a failure");

  gBrowser.removeCurrentTab();
  Harness.finish();
};
// ----------------------------------------------------------------------------
