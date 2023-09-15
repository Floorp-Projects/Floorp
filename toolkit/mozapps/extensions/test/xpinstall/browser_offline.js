var proxyPrefValue;

// ----------------------------------------------------------------------------
// Tests that going offline cancels an in progress download.
function test() {
  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.downloadProgressCallback = download_progress;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function download_progress(addon, value, maxValue) {
  try {
    // Tests always connect to localhost, and per bug 87717, localhost is now
    // reachable in offline mode.  To avoid this, disable any proxy.
    proxyPrefValue = Services.prefs.getIntPref("network.proxy.type");
    Services.prefs.setIntPref("network.proxy.type", 0);
    Services.io.manageOfflineStatus = false;
    Services.io.offline = true;
  } catch (ex) {}
}

function finish_test(count) {
  function wait_for_online() {
    info("Checking if the browser is still offline...");

    let tab = gBrowser.selectedTab;
    BrowserTestUtils.waitForContentEvent(
      tab.linkedBrowser,
      "DOMContentLoaded",
      true
    ).then(async function () {
      let url = await ContentTask.spawn(
        tab.linkedBrowser,
        null,
        async function () {
          return content.document.documentURI;
        }
      );
      info("loaded: " + url);
      if (/^about:neterror\?e=netOffline/.test(url)) {
        wait_for_online();
      } else {
        gBrowser.removeCurrentTab();
        Harness.finish();
      }
    });
    BrowserTestUtils.startLoadingURIString(
      tab.linkedBrowser,
      "http://example.com/"
    );
  }

  is(count, 0, "No add-ons should have been installed");
  try {
    Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);
    Services.io.offline = false;
  } catch (ex) {}

  PermissionTestUtils.remove("http://example.com", "install");

  wait_for_online();
}
