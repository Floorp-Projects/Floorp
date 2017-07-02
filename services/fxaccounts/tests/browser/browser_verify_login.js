/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

add_task(async function() {
  let payload = {
    data: {
      deviceName: "Laptop",
      url: "http://localhost/newLogin",
      title: "Sign-in Request",
      body: "New sign-in request from vershwal's Nighty on Intel Mac OS X 10.12"
    }
  };
  info("testVerifyNewSignin");
  setupMockAlertsService();
  gBrowser.selectedBrowser.loadURI("about:robots");
  await waitForDocLoadComplete();

  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  Services.obs.notifyObservers(null, "fxaccounts:verify_login", JSON.stringify(payload.data));

  let tab = await waitForTabPromise;
  Assert.ok("Tab successfully opened");
  Assert.equal(tab.linkedBrowser.currentURI.spec, payload.data.url);
  await BrowserTestUtils.removeTab(tab);
});
