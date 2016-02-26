// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an installChrome call in web
// content. This should be blocked by the whitelist check.
// This verifies bug 252830
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installchrome.html? " + encodeURIComponent(TESTROOT + "amosigned.xpi"));
}

function allow_blocked(installInfo) {
  is(installInfo.browser, gBrowser.selectedBrowser, "Install should have been triggered by the right browser");
  is(installInfo.originatingURI.spec, gBrowser.currentURI.spec, "Install should have been triggered by the right uri");
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
