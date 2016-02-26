// ----------------------------------------------------------------------------
// Test that hashes are ignored in the headers of HTTP requests
// This verifies bug 591070
function test() {
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var url = "http://example.com/browser/" + RELATIVE_DIR + "hashRedirect.sjs";
  url += "?sha1:foobar|" + TESTROOT + "amosigned.xpi";

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

  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
