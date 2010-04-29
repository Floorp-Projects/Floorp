// ----------------------------------------------------------------------------
// Test whether an install succeeds when a valid hash is included
// This verifies bug 302284
function test() {
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: TESTROOT + "unsigned.xpi",
      Hash: "sha1:3d0dc22e1f394e159b08aaf5f0f97de4d5c65f4f",
      toString: function() { return this.URL; }
    }
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
