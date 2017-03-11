/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const { MockRegistrar } =
  Cu.import("resource://testing-common/MockRegistrar.jsm", {});
const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"]
                     .getService(Ci.nsIObserver);
const accountsBundle = Services.strings.createBundle(
  "chrome://browser/locale/accounts.properties"
);
const DEVICES_URL = "http://localhost/devices";

let expectedBody;

add_task(async function setup() {
  let fxAccounts = {
    promiseAccountsManageDevicesURI() {
      return Promise.resolve(DEVICES_URL);
    }
  };
  gBrowserGlue.observe({wrappedJSObject: fxAccounts}, "browser-glue-test", "mock-fxaccounts");
  const alertsService = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAlertsService, Ci.nsISupports]),
    showAlertNotification: (image, title, text, clickable, cookie, clickCallback) => {
      // We can't simulate a click on the alert popup,
      // so instead we call the click listener ourselves directly
      clickCallback.observe(null, "alertclickcallback", null);
      Assert.equal(text, expectedBody);
    }
  };
  let alertsServiceCID = MockRegistrar.register("@mozilla.org/alerts-service;1", alertsService);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(alertsServiceCID);
  });
});

async function testDeviceConnected(deviceName) {
  info("testDeviceConnected with deviceName=" + deviceName);
  gBrowser.selectedBrowser.loadURI("about:robots");
  await waitForDocLoadComplete();

  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  Services.obs.notifyObservers(null, "fxaccounts:device_connected", deviceName);

  let tab = await waitForTabPromise;
  Assert.ok("Tab successfully opened");

  Assert.equal(tab.linkedBrowser.currentURI.spec, DEVICES_URL);

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function() {
  expectedBody = accountsBundle.formatStringFromName("deviceConnectedBody", ["My phone"], 1);
  await testDeviceConnected("My phone");
});

add_task(async function() {
  expectedBody = accountsBundle.GetStringFromName("deviceConnectedBody.noDeviceName");
  await testDeviceConnected(null);
});
