// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through a navigation. Should not be
// blocked since the referer is whitelisted.
function test() {
  ignoreAllUncaughtExceptions();
  Harness.installConfirmCallback = confirm_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT2 + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "unsigned.xpi", makeURI(TESTROOT2 + "test.html"));
}

function confirm_install(window) {
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove("example.org", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
