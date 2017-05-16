// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through a navigation. Should be
// blocked since the referer is not whitelisted even though the target is.
var url = TESTROOT2 + "navigate.html?" + encodeURIComponent(TESTROOT + "amosigned.xpi");

function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.loadURI(url);
}

function allow_blocked(installInfo) {
  is(installInfo.browser, gBrowser.selectedBrowser, "Install should have been triggered by the right browser");
  is(installInfo.originatingURI.spec, url, "Install should have been triggered by the right uri");
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
