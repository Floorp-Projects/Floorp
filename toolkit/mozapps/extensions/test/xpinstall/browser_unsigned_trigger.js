// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an InstallTrigger call in web
// content.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: TESTROOT + "unsigned.xpi",
      IconURL: TESTROOT + "icon.png",
      toString() { return this.URL; }
    }
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  var items = window.document.getElementById("itemList").childNodes;
  is(items.length, 1, "Should only be 1 item listed in the confirmation dialog");
  is(items[0].name, "XPI Test", "Should have seen the name");
  is(items[0].url, TESTROOT + "unsigned.xpi", "Should have listed the correct url for the item");
  is(items[0].icon, TESTROOT + "icon.png", "Should have listed the correct icon for the item");
  is(items[0].signed, "false", "Should have listed the item as unsigned");
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

const finish_test = Task.async(function*(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  Services.perms.remove(makeURI("http://example.com"), "install");

  const results = yield ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    return {
      return: content.document.getElementById("return").textContent,
      status: content.document.getElementById("status").textContent,
    }
  })

  is(results.return, "true", "installTrigger should have claimed success");
  is(results.status, "0", "Callback should have seen a success");

  gBrowser.removeCurrentTab();
  Harness.finish();
});
