// ----------------------------------------------------------------------------
// Test whether an install succeeds when a valid hash is included
// This verifies bug 302284
function test() {
  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: TESTROOT + "amosigned.xpi",
        Hash: "sha1:ee95834ad862245a9ef99ccecc2a857cadc16404",
        toString() {
          return this.URL;
        },
      },
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function install_ended(install, addon) {
  return addon.uninstall();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  PermissionTestUtils.remove("http://example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
