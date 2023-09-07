/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", false],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

// Tests a stateless bounce via client redirect.
add_task(async function test_client_bounce_simple() {
  await runTestBounce({ bounceType: "client" });
});

// Tests a stateless bounce via server redirect.
add_task(async function test_server_bounce_simple() {
  await runTestBounce({ bounceType: "server" });
});

// Tests a chained redirect consisting of a server and a client redirect.
add_task(async function test_bounce_chain() {
  Assert.equal(
    bounceTrackingProtection.bounceTrackerCandidateHosts.length,
    0,
    "No bounce tracker hosts initially."
  );
  Assert.equal(
    bounceTrackingProtection.userActivationHosts.length,
    0,
    "No user activation hosts initially."
  );

  await BrowserTestUtils.withNewTab(
    getBaseUrl(ORIGIN_A) + "file_start.html",
    async browser => {
      let promiseRecordBounces = waitForRecordBounces(browser);

      // The final destination after the bounces.
      let targetURL = new URL(getBaseUrl(ORIGIN_B) + "file_start.html");

      // Construct last hop.
      let bounceChainUrlEnd = getBounceURL({ bounceType: "server", targetURL });
      // Construct first hop, nesting last hop.
      let bounceChainUrlFull = getBounceURL({
        bounceType: "client",
        redirectDelayMS: 100,
        bounceOrigin: ORIGIN_TRACKER_B,
        targetURL: bounceChainUrlEnd,
      });

      info("bounceChainUrl: " + bounceChainUrlFull.href);

      // Navigate through the bounce chain.
      await navigateLinkClick(browser, bounceChainUrlFull);

      // Wait for the final site to be loaded which complete the BounceTrackingRecord.
      await BrowserTestUtils.browserLoaded(browser, false, targetURL);

      // Navigate again with user gesture which triggers
      // BounceTrackingProtection::RecordStatefulBounces. We could rely on the
      // timeout (mClientBounceDetectionTimeout) here but that can cause races
      // in debug where the load is quite slow.
      await navigateLinkClick(
        browser,
        new URL(getBaseUrl(ORIGIN_C) + "file_start.html")
      );

      await promiseRecordBounces;

      Assert.deepEqual(
        bounceTrackingProtection.bounceTrackerCandidateHosts.sort(),
        [SITE_TRACKER_B, SITE_TRACKER].sort(),
        `Identified all bounce trackers in the redirect chain.`
      );
      Assert.deepEqual(
        bounceTrackingProtection.userActivationHosts.sort(),
        [SITE_A, SITE_B].sort(),
        "Should only have user activation for sites where we clicked links."
      );

      bounceTrackingProtection.reset();
    }
  );
});
