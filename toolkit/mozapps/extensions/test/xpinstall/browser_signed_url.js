// ----------------------------------------------------------------------------
// Tests installing an signed add-on by navigating directly to the url
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    gBrowser.loadURI(TESTROOT + "signed.xpi");
  });
}

function confirm_install(window) {
  let items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should only be 1 item listed in the confirmation dialog");
  is(items[0].name, "Signed XPI Test", "Should have had the name");
  is(items[0].url, TESTROOT + "signed.xpi", "Should have listed the correct url for the item");
  is(items[0].cert, "(Object Signer)", "Should have seen the signer");
  is(items[0].signed, "true", "Should have listed the item as signed");
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
