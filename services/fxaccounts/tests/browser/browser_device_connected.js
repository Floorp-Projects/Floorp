/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const { MockRegistrar } =
  Cu.import("resource://testing-common/MockRegistrar.jsm", {});

let accountsBundle = Services.strings.createBundle(
  "chrome://browser/locale/accounts.properties"
);

let expectedBody;

add_task(async function setup() {
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

  Preferences.set("identity.fxaccounts.settings.devices.uri", "http://localhost/devices");

  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  Services.obs.notifyObservers(null, "fxaccounts:device_connected", deviceName);

  let tab = await waitForTabPromise;
  Assert.ok("Tab successfully opened");

  let expectedURI = Preferences.get("identity.fxaccounts.settings.devices.uri",
                                    "prefundefined");
  Assert.equal(tab.linkedBrowser.currentURI.spec, expectedURI);

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
