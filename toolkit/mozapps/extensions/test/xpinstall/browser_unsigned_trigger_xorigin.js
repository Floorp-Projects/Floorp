// ----------------------------------------------------------------------------
// Ensure that an inner frame from a different origin can't initiate an install

var wasOriginBlocked = false;

function test() {
  Harness.installOriginBlockedCallback = install_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var inner_url = encodeURIComponent(TESTROOT + "installtrigger.html?" + encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: TESTROOT + "amosigned.xpi",
      IconURL: TESTROOT + "icon.png",
      toString() { return this.URL; }
    }
  })));
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.loadURI(TESTROOT2 + "installtrigger_frame.html?" + inner_url);
}

function install_blocked(installInfo) {
  wasOriginBlocked = true;
}

function finish_test(count) {
  ok(wasOriginBlocked, "Should have been blocked due to the cross origin request.");

  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
