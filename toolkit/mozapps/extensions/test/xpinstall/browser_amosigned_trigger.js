/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an InstallTrigger call in web
// content.
function test() {
  // This test depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

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
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
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
  is(count, 1, "1 Add-on should have been successfully installed");

  PermissionTestUtils.remove("http://example.com", "install");

  const results = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
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

  gBrowser.removeCurrentTab();
  Harness.finish();
};
