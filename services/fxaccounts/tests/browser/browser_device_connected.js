/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);
const DEVICES_URL = "https://example.com/devices";

add_setup(async function () {
  const origManageDevicesURI = FxAccounts.config.promiseManageDevicesURI;
  FxAccounts.config.promiseManageDevicesURI = () =>
    Promise.resolve(DEVICES_URL);
  setupMockAlertsService();

  registerCleanupFunction(function () {
    FxAccounts.config.promiseManageDevicesURI = origManageDevicesURI;
    delete window.FxAccounts;
  });
});

async function testDeviceConnected(deviceName) {
  info("testDeviceConnected with deviceName=" + deviceName);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:mozilla"
  );
  await waitForDocLoadComplete();

  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  Services.obs.notifyObservers(null, "fxaccounts:device_connected", deviceName);

  let tab = await waitForTabPromise;
  Assert.ok("Tab successfully opened");

  Assert.equal(tab.linkedBrowser.currentURI.spec, DEVICES_URL);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function () {
  await testDeviceConnected("My phone");
});

add_task(async function () {
  await testDeviceConnected(null);
});
