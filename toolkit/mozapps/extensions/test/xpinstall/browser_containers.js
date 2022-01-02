/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

const MY_CONTEXT = 2;
let gDidSeeChannel = false;

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
    loadInfo.originAttributes.userContextId,
    MY_CONTEXT,
    "Got expected usercontextid"
  );
}
// ----------------------------------------------------------------------------
// Tests we send the right cookies when installing through an InstallTrigger call
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://example.com/"),
    { userContextId: MY_CONTEXT }
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
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "", {
    userContextId: MY_CONTEXT,
  });
  Services.obs.addObserver(check_channel, "http-on-before-connect");
  BrowserTestUtils.loadURI(
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

const finish_test = async function(count) {
  ok(
    gDidSeeChannel,
    "Should have seen the request for the XPI and verified it was sent the right way."
  );
  is(count, 1, "1 Add-on should have been successfully installed");

  Services.obs.removeObserver(check_channel, "http-on-before-connect");

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
