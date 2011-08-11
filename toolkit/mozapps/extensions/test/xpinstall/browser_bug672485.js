/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installFailedCallback = failed_install;
  Harness.installEndedCallback = complete_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();
  
  delete Services.ww;
  is(Services.ww, undefined, "Services.ww should now be undefined");

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  ok(false, "Should not see the install dialog");
  return false;
}

function failed_install() {
  ok(true, "Install should fail");
}

function complete_install() {
  ok(false, "Install should not have completed");
  return false;
}

function finish_test(count) {
  is(count, 0, "0 Add-ons should have been successfully installed");

  gBrowser.removeCurrentTab();
  
  Services.ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                  .getService(Ci.nsIWindowWatcher);
  
  Harness.finish();
}
