// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through a startSoftwareUpdate call in web
// content. This should be blocked by the whitelist check.
// This verifies bug 252830
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "startsoftwareupdate.html? " + encodeURIComponent(TESTROOT + "unsigned.xpi"));
}

function allow_blocked(installInfo) {
  is(installInfo.originatingWindow, gBrowser.contentWindow, "Install should have been triggered by the right window");
  is(installInfo.originatingURI.spec, gBrowser.currentURI.spec, "Install should have been triggered by the right uri");
  return false;
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
