/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
  bounceTrackingProtection = Cc[
    "@mozilla.org/bounce-tracking-protection;1"
  ].getService(Ci.nsIBounceTrackingProtection);
});

async function runTest(spawnWindowType) {
  if (!spawnWindowType || !["newTab", "popup"].includes(spawnWindowType)) {
    throw new Error(`Invalid option '${spawnWindowType}' for spawnWindowType`);
  }

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
    0,
    "No bounce tracker hosts initially."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    0,
    "No user activation hosts initially."
  );

  // Spawn a tab with A, the start of the bounce chain.
  await BrowserTestUtils.withNewTab(
    getBaseUrl(ORIGIN_A) + "file_start.html",
    async browser => {
      // The destination site C to navigate to after the bounce.
      let finalURL = new URL(getBaseUrl(ORIGIN_B) + "file_start.html");
      // The middle hop in the bounce chain B that redirects to finalURL C.
      let bounceURL = getBounceURL({
        bounceType: "client",
        targetURL: finalURL,
        setState: "cookie-client",
      });

      // Register a promise for the new popup window. This resolves once the popup
      // has opened and the final url (C) has been loaded.
      let openPromise;

      if (spawnWindowType == "newTab") {
        openPromise = BrowserTestUtils.waitForNewTab(gBrowser, finalURL.href);
      } else {
        openPromise = BrowserTestUtils.waitForNewWindow({ url: finalURL.href });
      }

      // Navigate through the bounce chain by opening a popup to the bounce URL.
      await navigateLinkClick(browser, bounceURL, {
        spawnWindow: spawnWindowType,
      });

      let tabOrWindow = await openPromise;

      let tabOrWindowBrowser;
      if (spawnWindowType == "newTab") {
        tabOrWindowBrowser = tabOrWindow.linkedBrowser;
      } else {
        tabOrWindowBrowser = tabOrWindow.gBrowser.selectedBrowser;
      }

      let promiseRecordBounces = waitForRecordBounces(tabOrWindowBrowser);

      // Navigate again with user gesture which triggers
      // BounceTrackingProtection::RecordStatefulBounces. We could rely on the
      // timeout (mClientBounceDetectionTimeout) here but that can cause races
      // in debug where the load is quite slow.
      await navigateLinkClick(
        tabOrWindowBrowser,
        new URL(getBaseUrl(ORIGIN_C) + "file_start.html")
      );

      info("Wait for bounce trackers to be recorded.");
      await promiseRecordBounces;

      // Cleanup popup or tab.
      if (spawnWindowType == "newTab") {
        await BrowserTestUtils.removeTab(tabOrWindow);
      } else {
        await BrowserTestUtils.closeWindow(tabOrWindow);
      }
    }
  );

  // Check that the bounce tracker was detected.
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts({})
      .map(entry => entry.siteHost),
    [SITE_TRACKER],
    "Bounce tracker in popup detected."
  );

  // Cleanup.
  bounceTrackingProtection.clearAll();
  await SiteDataTestUtils.clear();
}

/**
 * Tests that bounce trackers which use popups as the first hop in the bounce
 * chain can not bypass detection.
 *
 * A -> popup -> B -> C
 *
 * A opens a popup and loads B in it. B is the tracker that performs a
 * short-lived redirect and C is the final destination.
 */

add_task(async function test_popup() {
  await runTest("popup");
});

add_task(async function test_new_tab() {
  await runTest("newTab");
});
