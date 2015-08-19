// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through a navigation. Should not be
// blocked since the referer is whitelisted.
let URL = TESTROOT2 + "navigate.html?" + encodeURIComponent(TESTROOT + "unsigned.xpi");

function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(URL);
}

function confirm_install(window) {
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove(makeURI("http://example.org"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
