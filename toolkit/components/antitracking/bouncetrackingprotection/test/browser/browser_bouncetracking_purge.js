/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BOUNCE_TRACKING_GRACE_PERIOD_SEC = 30;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec",
        BOUNCE_TRACKING_GRACE_PERIOD_SEC,
      ],
      ["privacy.bounceTrackingProtection.requireStatefulBounces", false],
    ],
  });
});

/**
 * The following tests ensure that sites that have open tabs are exempt from purging.
 */

function initBounceTrackerState() {
  bounceTrackingProtection.reset();

  // Bounce time of 0 is out of the grace period which means we should purge.
  bounceTrackingProtection.testAddBounceTrackerCandidate("example.com", 0);
  bounceTrackingProtection.testAddBounceTrackerCandidate("example.net", 0);

  // Should not purge because within grace period.
  let timestampWithinGracePeriod =
    Date.now() - (BOUNCE_TRACKING_GRACE_PERIOD_SEC * 1000) / 2;
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    "example.org",
    timestampWithinGracePeriod * 1000
  );
}

add_task(async function test_purging_skip_open_foreground_tab() {
  initBounceTrackerState();

  // Foreground tab
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  Assert.deepEqual(
    await bounceTrackingProtection.testRunPurgeBounceTrackers(),
    ["example.net"],
    "Should only purge example.net. example.org is within the grace period, example.com has an open tab."
  );

  info("Close the tab for example.com and test that it gets purged now.");
  initBounceTrackerState();

  BrowserTestUtils.removeTab(tab);
  Assert.deepEqual(
    (await bounceTrackingProtection.testRunPurgeBounceTrackers()).sort(),
    ["example.net", "example.com"].sort(),
    "example.com should have been purged now that it no longer has an open tab."
  );

  bounceTrackingProtection.reset();
});

add_task(async function test_purging_skip_open_background_tab() {
  initBounceTrackerState();

  // Background tab
  let tab = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  Assert.deepEqual(
    await bounceTrackingProtection.testRunPurgeBounceTrackers(),
    ["example.net"],
    "Should only purge example.net. example.org is within the grace period, example.com has an open tab."
  );

  info("Close the tab for example.com and test that it gets purged now.");
  initBounceTrackerState();

  BrowserTestUtils.removeTab(tab);
  Assert.deepEqual(
    (await bounceTrackingProtection.testRunPurgeBounceTrackers()).sort(),
    ["example.net", "example.com"].sort(),
    "example.com should have been purged now that it no longer has an open tab."
  );

  bounceTrackingProtection.reset();
});

add_task(async function test_purging_skip_open_tab_extra_window() {
  initBounceTrackerState();

  // Foreground tab in new window.
  let win = await BrowserTestUtils.openNewBrowserWindow({});
  await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "https://example.com"
  );
  Assert.deepEqual(
    await bounceTrackingProtection.testRunPurgeBounceTrackers(),
    ["example.net"],
    "Should only purge example.net. example.org is within the grace period, example.com has an open tab."
  );

  info(
    "Close the window with the tab for example.com and test that it gets purged now."
  );
  initBounceTrackerState();

  await BrowserTestUtils.closeWindow(win);
  Assert.deepEqual(
    (await bounceTrackingProtection.testRunPurgeBounceTrackers()).sort(),
    ["example.net", "example.com"].sort(),
    "example.com should have been purged now that it no longer has an open tab."
  );

  bounceTrackingProtection.reset();
});
