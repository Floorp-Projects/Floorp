/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gWindowWatcher = null;

function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installCancelledCallback = cancelled_install;
  Harness.installEndedCallback = complete_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gWindowWatcher = Services.ww;
  delete Services.ww;
  is(Services.ww, undefined, "Services.ww should now be undefined");

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function confirm_install(panel) {
  ok(false, "Should not see the install dialog");
  return false;
}

function cancelled_install() {
  ok(true, "Install should b cancelled");
}

function complete_install() {
  ok(false, "Install should not have completed");
  return false;
}

function finish_test(count) {
  is(count, 0, "0 Add-ons should have been successfully installed");

  gBrowser.removeCurrentTab();

  Services.ww = gWindowWatcher;

  Services.perms.remove(makeURI("http://example.com"), "install");

  Harness.finish();
}
