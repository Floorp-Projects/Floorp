/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ORIGIN = "https://itisatracker.org";
const TEST_BASE_DOMAIN = "itisatracker.org";

async function runPurgeTest(expectPurge) {
  ok(!SiteDataTestUtils.hasCookies(TEST_ORIGIN), "No cookies initially.");

  // Adding a cookie that should later be purged.
  info("Add a test cookie to be purged later.");
  SiteDataTestUtils.addToCookies({ origin: TEST_ORIGIN });
  ok(SiteDataTestUtils.hasCookies(TEST_ORIGIN), "Cookie added.");

  // The bounce adds localStorage. Test that there is none initially.
  ok(
    !SiteDataTestUtils.hasLocalStorage(TEST_ORIGIN),
    "No localStorage initially."
  );

  info("Test client bounce with cookie.");
  await runTestBounce({
    bounceType: "client",
    setState: "localStorage",
    skipSiteDataCleanup: true,
    postBounceCallback: () => {
      info(
        "Test that after the bounce but before purging cookies and localStorage are present."
      );
      ok(SiteDataTestUtils.hasCookies(TEST_ORIGIN), "Cookies not purged.");
      ok(
        SiteDataTestUtils.hasLocalStorage(TEST_ORIGIN),
        "localStorage not purged."
      );

      Assert.deepEqual(
        bounceTrackingProtection
          .testGetBounceTrackerCandidateHosts({})
          .map(entry => entry.siteHost),
        [TEST_BASE_DOMAIN],
        `Bounce tracker candidate '${TEST_BASE_DOMAIN}' added`
      );
    },
  });

  if (expectPurge) {
    info("After purging the site shouldn't have any data.");
    ok(!SiteDataTestUtils.hasCookies(TEST_ORIGIN), "Cookies purged.");
    ok(!SiteDataTestUtils.hasLocalStorage(TEST_ORIGIN), "localStorage purged.");
  } else {
    info("Purging did not run meaning the site should still have data.");

    ok(SiteDataTestUtils.hasCookies(TEST_ORIGIN), "Cookies still set.");
    ok(
      SiteDataTestUtils.hasLocalStorage(TEST_ORIGIN),
      "localStorage still set."
    );
  }

  info(
    "Candidates should have been removed after running the purging algorithm. This is true for both regular and dry-run mode where we pretend to purge."
  );
  Assert.deepEqual(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}),
    [],
    "No bounce tracker candidates after purging."
  );

  // Cleanup.
  bounceTrackingProtection.clearAll();
  await SiteDataTestUtils.clear();
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
      // Required to use SiteDataTestUtils localStorage helpers.
      ["dom.storage.client_validation", false],
    ],
  });
});

add_task(async function test_purge_in_regular_mode() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.bounceTrackingProtection.enableDryRunMode", false]],
  });

  await runPurgeTest(true);
});

add_task(async function test_purge_in_dry_run_mode() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.bounceTrackingProtection.enableDryRunMode", true]],
  });

  await runPurgeTest(false);
});
