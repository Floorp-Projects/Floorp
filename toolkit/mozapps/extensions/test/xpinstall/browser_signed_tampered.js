// ----------------------------------------------------------------------------
// Tests installing a signed add-on that has been tampered with after signing.
// In "no signature required" mode, a tampered add-on is equivalent to an
// unsigned add-on.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Tampered Signed XPI": TESTROOT + "signed-tampered.xpi"
  }));
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  var items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should only be 1 item listed in the confirmation dialog");
  is(items[0].name, "Signed XPI Test - Tampered", "Should have seen the name");
  is(items[0].url, TESTROOT + "signed-tampered.xpi", "Should have listed the correct url for the item");
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

const finish_test = async function(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  Services.perms.remove(makeURI("http://example.com"), "install");

  const results = await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    return {
      return: content.document.getElementById("return").textContent,
      status: content.document.getElementById("status").textContent,
    };
  });

  is(results.return, "true", "installTrigger should have claimed success");
  is(results.status, "0", "Callback should have seen a success");
  gBrowser.removeCurrentTab();
  Harness.finish();
};
