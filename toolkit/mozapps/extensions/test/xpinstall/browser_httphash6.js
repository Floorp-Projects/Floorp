// ----------------------------------------------------------------------------
// Tests that a new hash is accepted when restarting a failed download
// This verifies bug 593535
function setup_redirect(aSettings) {
  var url = "https://example.com/browser/" + RELATIVE_DIR + "redirect.sjs?mode=setup";
  for (var name in aSettings) {
    url += "&" + name + "=" + aSettings[name];
  }

  var req = new XMLHttpRequest();
  req.open("GET", url, false);
  req.send(null);
}

var gInstall = null;

function test() {
  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_failed_download;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);

  // Set up the redirect to give a bad hash
  setup_redirect({
    "X-Target-Digest": "sha1:foo",
    "Location": "http://example.com/browser/" + RELATIVE_DIR + "unsigned.xpi"
  });

  var url = "https://example.com/browser/" + RELATIVE_DIR + "redirect.sjs?mode=redirect";

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: url,
      toString: function() { return this.URL; }
    }
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_INCORRECT_HASH, "Should have seen a hash failure");
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
    "X-Target-Digest": "sha1:3d0dc22e1f394e159b08aaf5f0f97de4d5c65f4f",
    "Location": "http://example.com/browser/" + RELATIVE_DIR + "unsigned.xpi"
  });

  // The harness expects onNewInstall events for all installs that are about to start
  Harness.onNewInstall(gInstall);

  // Restart the install as a regular webpage install so the harness tracks it
  AddonManager.installAddonsFromWebpage("application/x-xpinstall",
                                        gBrowser.contentWindow,
                                        gBrowser.currentURI, [gInstall]);
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  Services.perms.remove("example.com", "install");
  Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);

  gBrowser.removeCurrentTab();
  Harness.finish();
}
