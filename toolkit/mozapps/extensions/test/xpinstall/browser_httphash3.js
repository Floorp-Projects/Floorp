// ----------------------------------------------------------------------------
// Tests that the HTTPS hash is ignored when InstallTrigger is passed a hash.
// This verifies bug 591070
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
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);

  var url = "https://example.com/browser/" + RELATIVE_DIR + "hashRedirect.sjs";
  url += "?sha1:foobar|" + TESTROOT + "amosigned.xpi";

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: url,
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
  Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);

  gBrowser.removeCurrentTab();
  Harness.finish();
}
