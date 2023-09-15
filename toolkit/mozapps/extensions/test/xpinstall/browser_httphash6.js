// ----------------------------------------------------------------------------
// Tests that a new hash is accepted when restarting a failed download
// This verifies bug 593535
function setup_redirect(aSettings) {
  var url =
    "https://example.com/browser/" + RELATIVE_DIR + "redirect.sjs?mode=setup";
  for (var name in aSettings) {
    url += "&" + name + "=" + aSettings[name];
  }

  var req = new XMLHttpRequest();
  req.open("GET", url, false);
  req.send(null);
}

var gInstall = null;

function test() {
  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_failed_download;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);

  // Set up the redirect to give a bad hash
  setup_redirect({
    "X-Target-Digest": "sha1:foo",
    Location: "http://example.com/browser/" + RELATIVE_DIR + "amosigned.xpi",
  });

  var url =
    "https://example.com/browser/" +
    RELATIVE_DIR +
    "redirect.sjs?mode=redirect";

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: url,
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

function download_failed(install) {
  is(
    install.error,
    AddonManager.ERROR_INCORRECT_HASH,
    "Should have seen a hash failure"
  );
  // Stash the failed download while the harness cleans itself up
  gInstall = install;
}

function finish_failed_download() {
  // Setup to track the successful re-download
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  // Give it the right hash this time
  setup_redirect({
    "X-Target-Digest": "sha1:ee95834ad862245a9ef99ccecc2a857cadc16404",
    Location: "http://example.com/browser/" + RELATIVE_DIR + "amosigned.xpi",
  });

  // The harness expects onNewInstall events for all installs that are about to start
  Harness.onNewInstall(gInstall);

  // Restart the install as a regular webpage install so the harness tracks it
  AddonManager.installAddonFromWebpage(
    "application/x-xpinstall",
    gBrowser.selectedBrowser,
    gBrowser.contentPrincipal,
    gInstall
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
