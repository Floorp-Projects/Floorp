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

  const trigger = encodeURIComponent(JSON.stringify({
    "Signed XPI (O)": TESTROOT + "signed-no-cn.xpi",
  }));
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + trigger);
}

function confirm_install(window) {
  let items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should be 1 item listed in the confirmation dialog");
  let item = items[0];
  is(item.name, "Signed XPI Test (No Common Name)", "Should have seen the name from the trigger list");
  is(item.cert, "(Mozilla Testing)", "Should have seen the signer");
  is(item.signed, "true", "Should have listed the item as signed");
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 add-on should have been successfully installed");

  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
