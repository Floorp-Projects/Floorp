/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);
const accountsBundle = Services.strings.createBundle(
  "chrome://browser/locale/accounts.properties"
);
const DEVICES_URL = "https://example.com/devices";

let expectedBody;

add_task(async function setup() {
  const origManageDevicesURI = FxAccounts.config.promiseManageDevicesURI;
  FxAccounts.config.promiseManageDevicesURI = () =>
    Promise.resolve(DEVICES_URL);
  setupMockAlertsService();

  registerCleanupFunction(function() {
    FxAccounts.config.promiseManageDevicesURI = origManageDevicesURI;
    delete window.FxAccounts;
  });
});

async function testDeviceConnected(deviceName) {
  info("testDeviceConnected with deviceName=" + deviceName);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:robots");
  await waitForDocLoadComplete();

  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  Services.obs.notifyObservers(null, "fxaccounts:device_connected", deviceName);

  let tab = await waitForTabPromise;
  Assert.ok("Tab successfully opened");

  Assert.equal(tab.linkedBrowser.currentURI.spec, DEVICES_URL);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function() {
  expectedBody = accountsBundle.formatStringFromName(
    "otherDeviceConnectedBody",
    ["My phone"]
  );
  await testDeviceConnected("My phone");
});

add_task(async function() {
  expectedBody = accountsBundle.GetStringFromName(
    "otherDeviceConnectedBody.noDeviceName"
  );
  await testDeviceConnected(null);
});
