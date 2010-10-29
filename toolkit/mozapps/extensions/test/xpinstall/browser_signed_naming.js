// ----------------------------------------------------------------------------
// Tests that the correct signer is presented for combinations of O and CN present.
// The signed files have (when present) O=Mozilla Testing, CN=Object Signer
// This verifies bug 372980
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Signed XPI (O and CN)": TESTROOT + "signed.xpi",
    "Signed XPI (CN)": TESTROOT + "signed-no-o.xpi",
    "Signed XPI (O)": TESTROOT + "signed-no-cn.xpi",
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function get_item(items, url) {
  for (let i = 0; i < items.length; i++) {
    if (items[i].url == url)
      return items[i];
  }
  ok(false, "Item for " + url + " was not listed");
  return null;
}

function confirm_install(window) {
  items = window.document.getElementById("itemList").childNodes;
  is(items.length, 3, "Should be 3 items listed in the confirmation dialog");
  let item = get_item(items, TESTROOT + "signed.xpi");
  if (item) {
    is(item.name, "Signed XPI Test", "Should have seen the name from the trigger list");
    is(item.cert, "(Object Signer)", "Should have seen the signer");
    is(item.signed, "true", "Should have listed the item as signed");
  }
  item = get_item(items, TESTROOT + "signed-no-o.xpi");
  if (item) {
    is(item.name, "Signed XPI Test (No Org)", "Should have seen the name from the trigger list");
    is(item.cert, "(Object Signer)", "Should have seen the signer");
    is(item.signed, "true", "Should have listed the item as signed");
  }
  item = get_item(items, TESTROOT + "signed-no-cn.xpi");
  if (item) {
    is(item.name, "Signed XPI Test (No Common Name)", "Should have seen the name from the trigger list");
    is(item.cert, "(Mozilla Testing)", "Should have seen the signer");
    is(item.signed, "true", "Should have listed the item as signed");
  }
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 3, "3 Add-ons should have been successfully installed");

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
