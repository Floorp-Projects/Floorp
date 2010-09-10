// ----------------------------------------------------------------------------
// Test whether an install succeeds when a valid hash is included in the HTTPS
// request
// This verifies bug 591070
function test() {
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);

  var url = "https://example.com/browser/" + RELATIVE_DIR + "hashRedirect.sjs";
  url += "?sha1:3d0dc22e1f394e159b08aaf5f0f97de4d5c65f4f|" + TESTROOT + "unsigned.xpi";

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: url,
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
  Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);

  gBrowser.removeCurrentTab();
  Harness.finish();
}
