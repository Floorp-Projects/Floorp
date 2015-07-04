// ----------------------------------------------------------------------------
// Test whether an install fails when an invalid hash is included
// This verifies bug 302284
function test() {
  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: TESTROOT + "unsigned.xpi",
      Hash: "sha1:643b08418599ddbd1ea8a511c90696578fb844b9",
      toString: function() { return this.URL; }
    }
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_INCORRECT_HASH, "Install should fail");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove(makeURI("http://example.com/"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
