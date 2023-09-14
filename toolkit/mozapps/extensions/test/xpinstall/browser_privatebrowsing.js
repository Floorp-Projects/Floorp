/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

let gDidSeeChannel = false;

AddonTestUtils.initMochitest(this);

function check_channel(subject) {
  if (!(subject instanceof Ci.nsIHttpChannel)) {
    return;
  }
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  let uri = channel.URI;
  if (!uri || !uri.spec.endsWith("amosigned.xpi")) {
    return;
  }
  gDidSeeChannel = true;
  ok(true, "Got request for " + uri.spec);

  let loadInfo = channel.loadInfo;
  is(
    loadInfo.originAttributes.privateBrowsingId,
    1,
    "Request should have happened using private browsing"
  );
}
// ----------------------------------------------------------------------------
// Tests we send the right cookies when installing through an InstallTrigger call
let gPrivateWin;
async function test() {
  waitForExplicitFinish(); // have to call this ourselves because we're async.

  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first_pbm", false]],
  });
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  gPrivateWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  Harness.setup(gPrivateWin);

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://example.com/"),
    { privateBrowsingId: 1 }
  );

  PermissionTestUtils.add(principal, "install", Services.perms.ALLOW_ACTION);

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: TESTROOT + "amosigned.xpi",
        IconURL: TESTROOT + "icon.png",
        toString() {
          return this.URL;
        },
      },
    })
  );
  gPrivateWin.gBrowser.selectedTab = BrowserTestUtils.addTab(
    gPrivateWin.gBrowser
  );
  Services.obs.addObserver(check_channel, "http-on-before-connect");
  BrowserTestUtils.startLoadingURIString(
    gPrivateWin.gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function confirm_install(panel) {
  is(panel.getAttribute("name"), "XPI Test", "Should have seen the name");
  return true;
}

function install_ended(install, addon) {
  AddonTestUtils.checkInstallInfo(install, {
    method: "installTrigger",
    source: "test-host",
    sourceURL: /http:\/\/example.com\/.*\/installtrigger.html/,
  });
  return addon.uninstall();
}

const finish_test = async function (count) {
  ok(
    gDidSeeChannel,
    "Should have seen the request for the XPI and verified it was sent the right way."
  );
  is(count, 1, "1 Add-on should have been successfully installed");

  Services.obs.removeObserver(check_channel, "http-on-before-connect");

  PermissionTestUtils.remove("http://example.com", "install");

  const results = await SpecialPowers.spawn(
    gPrivateWin.gBrowser.selectedBrowser,
    [],
    () => {
      return {
        return: content.document.getElementById("return").textContent,
        status: content.document.getElementById("status").textContent,
      };
    }
  );

  is(results.return, "true", "installTrigger should have claimed success");
  is(results.status, "0", "Callback should have seen a success");

  await TestUtils.waitForCondition(() =>
    gPrivateWin.AppMenuNotifications._notifications.some(
      n => n.id == "addon-installed"
    )
  );
  // Explicitly remove the notification to avoid the panel reopening in the
  // other window once this window closes (see also bug 1535069):
  gPrivateWin.AppMenuNotifications.removeNotification("addon-installed");

  // Now finish the test:
  await BrowserTestUtils.closeWindow(gPrivateWin);
  Harness.finish(gPrivateWin);
  gPrivateWin = null;
};
