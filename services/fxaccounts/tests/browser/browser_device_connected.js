/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const { MockRegistrar } =
  Cu.import("resource://testing-common/MockRegistrar.jsm", {});

const StubAlertsService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAlertsService, Ci.nsISupports]),

  showAlertNotification(image, title, text, clickable, cookie, clickCallback) {
    // We can't simulate a click on the alert popup,
    // so instead we call the click listener ourselves directly
    clickCallback.observe(null, "alertclickcallback", null);
  }
}

add_task(function*() {
  gBrowser.selectedBrowser.loadURI("about:robots");
  yield waitForDocLoadComplete();

  MockRegistrar.register("@mozilla.org/alerts-service;1", StubAlertsService);
  Preferences.set("identity.fxaccounts.settings.devices.uri", "http://localhost/devices");

  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  Services.obs.notifyObservers(null, "fxaccounts:device_connected", "My phone");

  let tab = yield waitForTabPromise;
  Assert.ok("Tab successfully opened");

  let expectedURI = Preferences.get("identity.fxaccounts.settings.devices.uri",
                                    "prefundefined");
  Assert.equal(tab.linkedBrowser.currentURI.spec, expectedURI);

  yield BrowserTestUtils.removeTab(tab);
});
