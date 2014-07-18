// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an InstallTrigger call in web
// content. This should be blocked by the whitelist check.
// This verifies bug 645699
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "bug645699.html");
}

function allow_blocked(installInfo) {
  is(installInfo.originator, gBrowser.contentWindow, "Install should have been triggered by the right window");
  is(installInfo.originatingURI.spec, gBrowser.currentURI.spec, "Install should have been triggered by the right uri");
  return false;
}

function confirm_install(window) {
  ok(false, "Should not see the install dialog");
  return false;
}

function finish_test(count) {
  is(count, 0, "0 Add-ons should have been successfully installed");
  Services.perms.remove("addons.mozilla.org", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
