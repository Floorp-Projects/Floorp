/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// ----------------------------------------------------------------------------
// Tests that cancelling multiple installs doesn't fail
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "amosigned.xpi",
    "Unsigned XPI 2": TESTROOT + "amosigned2.xpi",
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function get_item(items, url) {
  for (let item of items) {
    if (item.url == url)
      return item;
  }
  ok(false, "Item for " + url + " was not listed");
  return null;
}

function confirm_install(window) {
  let items = window.document.getElementById("itemList").childNodes;
  is(items.length, 2, "Should be 2 items listed in the confirmation dialog");
  let item = get_item(items, TESTROOT + "amosigned.xpi");
  if (item) {
    is(item.name, "XPI Test", "Should have seen the name from the trigger list");
    is(item.signed, "false", "Should have listed the item as signed");
  }
  item = get_item(items, TESTROOT + "amosigned2.xpi");
  if (item) {
    is(item.name, "Signed XPI Test", "Should have seen the name from the trigger list");
    is(item.signed, "false", "Should have listed the item as signed");
  }
  return false;
}

function install_ended(install, addon) {
  ok(false, "Should not have seen installs complete");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been successfully installed");

  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
