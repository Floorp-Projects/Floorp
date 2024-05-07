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
  bounceTrackingProtection.clearAll();

  // Bounce time of 1 is out of the grace period which means we should purge.
  bounceTrackingProtection.testAddBounceTrackerCandidate({}, "example.com", 1);
  bounceTrackingProtection.testAddBounceTrackerCandidate({}, "example.net", 1);

  // Should not purge because within grace period.
  let timestampWithinGracePeriod =
    Date.now() - (BOUNCE_TRACKING_GRACE_PERIOD_SEC * 1000) / 2;
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    {},
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

  bounceTrackingProtection.clearAll();
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

  bounceTrackingProtection.clearAll();
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

  bounceTrackingProtection.clearAll();
});

add_task(async function test_purging_skip_content_blocking_allow_list() {
  initBounceTrackerState();

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    window.ContentBlockingAllowList.add(browser);
  });

  Assert.deepEqual(
    await bounceTrackingProtection.testRunPurgeBounceTrackers(),
    ["example.net"],
    "Should only purge example.net. example.org is within the grace period, example.com is allow-listed."
  );

  // example.net is removed because it is purged, example.com is removed because
  // it is allow-listed.
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts({})
      .map(entry => entry.siteHost),
    ["example.org"],
    "Should have removed example.net and example.com from bounce tracker candidate list."
  );

  info("Add example.com as a bounce tracker candidate again.");
  bounceTrackingProtection.testAddBounceTrackerCandidate({}, "example.com", 1);

  info(
    "Remove the allow-list entry for example.com and test that it gets purged now."
  );

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    window.ContentBlockingAllowList.remove(browser);
  });
  Assert.deepEqual(
    await bounceTrackingProtection.testRunPurgeBounceTrackers(),
    ["example.com"],
    "example.com should have been purged now that it is no longer allow-listed."
  );

  // example.org is still in the grace period so it neither gets purged nor
  // removed from the candidate list.
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts({})
      .map(entry => entry.siteHost),
    ["example.org"],
    "Should have removed example.com from bounce tracker candidate list."
  );

  bounceTrackingProtection.clearAll();
});

add_task(
  async function test_purging_skip_content_blocking_allow_list_subdomain() {
    initBounceTrackerState();

    await BrowserTestUtils.withNewTab(
      "https://test1.example.com",
      async browser => {
        window.ContentBlockingAllowList.add(browser);
      }
    );

    Assert.deepEqual(
      await bounceTrackingProtection.testRunPurgeBounceTrackers(),
      ["example.net"],
      "Should only purge example.net. example.org is within the grace period, example.com is allow-listed via test1.example.com."
    );

    // example.net is removed because it is purged, example.com is removed because it is allow-listed.
    Assert.deepEqual(
      bounceTrackingProtection
        .testGetBounceTrackerCandidateHosts({})
        .map(entry => entry.siteHost),
      ["example.org"],
      "Should have removed example.net and example.com from bounce tracker candidate list."
    );

    info("Add example.com as a bounce tracker candidate again.");
    bounceTrackingProtection.testAddBounceTrackerCandidate(
      {},
      "example.com",
      1
    );

    info(
      "Remove the allow-list entry for test1.example.com and test that it gets purged now."
    );

    await BrowserTestUtils.withNewTab(
      "https://test1.example.com",
      async browser => {
        window.ContentBlockingAllowList.remove(browser);
      }
    );
    Assert.deepEqual(
      await bounceTrackingProtection.testRunPurgeBounceTrackers(),
      ["example.com"],
      "example.com should have been purged now that test1.example.com is no longer allow-listed."
    );

    bounceTrackingProtection.clearAll();
  }
);
