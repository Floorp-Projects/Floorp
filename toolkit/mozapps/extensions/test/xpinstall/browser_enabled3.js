// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.install call fails when xpinstall is disabled
function test() {
  Harness.installDisabledCallback = install_disabled;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installConfirmCallback = confirm_install;
  Harness.setup();

  Services.prefs.setBoolPref("xpinstall.enabled", false);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "amosigned.xpi"
  }));
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  ContentTask.spawn(gBrowser.selectedBrowser, TESTROOT + "installtrigger.html?" + triggers, url => {
    return new Promise(resolve => {
      function page_loaded() {
        content.removeEventListener("PageLoaded", page_loaded);
        resolve(content.document.getElementById("return").textContent);
      }

      function load_listener() {
        removeEventListener("load", load_listener, true);
        content.addEventListener("InstallTriggered", page_loaded);
      }

      addEventListener("load", load_listener, true);

      content.location.href = url;
    });
  }).then(text => {
    is(text, "false", "installTrigger should have not been enabled");
    Services.prefs.clearUserPref("xpinstall.enabled");
    gBrowser.removeCurrentTab();
    Harness.finish();
  });
}

function install_disabled(installInfo) {
  ok(true, "Saw installation disabled");
}

function allow_blocked(installInfo) {
  ok(false, "Should never see the blocked install notification");
  return false;
}

function confirm_install(window) {
  ok(false, "Should never see an install confirmation dialog");
  return false;
}
