/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const TOGGLE_HAS_USED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.has-used";
const TOGGLE_FIRST_SEEN_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.first-seen-secs";

/**
 * This tests that the first-time toggle doesn't change to the icon toggle.
 */
add_task(async function test_experiment_control_displayDuration() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_FIRST_SEEN_PREF, 0],
          [TOGGLE_HAS_USED_PREF, false],
        ],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      const hasUsed = Services.prefs.getBoolPref(TOGGLE_HAS_USED_PREF);
      const firstSeen = Services.prefs.getIntPref(TOGGLE_FIRST_SEEN_PREF);

      Assert.ok(!hasUsed, "has-used is false and toggle is not icon");
      Assert.notEqual(firstSeen, 0, "First seen should not be 0");
    }
  );
});

/**
 * This tests that the first-time toggle changes to the icon toggle
 * if the displayDuration end date is reached or passed.
 */
add_task(async function test_experiment_displayDuration_end_date_was_reached() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      displayDuration: 1,
    },
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_FIRST_SEEN_PREF, 222],
          [TOGGLE_HAS_USED_PREF, false],
        ],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      const hasUsed = Services.prefs.getBoolPref(TOGGLE_HAS_USED_PREF);
      const firstSeen = Services.prefs.getIntPref(TOGGLE_FIRST_SEEN_PREF);

      Assert.ok(hasUsed, "has-used is true and toggle is icon");
      Assert.equal(firstSeen, 222, "First seen should remain unchanged");
    }
  );

  doExperimentCleanup();
});

/**
 * This tests that the first-time toggle does not change to the icon toggle
 * if the displayDuration end date is not yet reached or passed.
 */
add_task(async function test_experiment_displayDuration_end_date_not_reached() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      displayDuration: 5,
    },
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      const currentDateSec = Math.round(Date.now() / 1000);

      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_FIRST_SEEN_PREF, currentDateSec],
          [TOGGLE_HAS_USED_PREF, false],
        ],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      const hasUsed = Services.prefs.getBoolPref(TOGGLE_HAS_USED_PREF);
      const firstSeen = Services.prefs.getIntPref(TOGGLE_FIRST_SEEN_PREF);

      Assert.ok(!hasUsed, "has-used is false and toggle is not icon");
      Assert.equal(
        firstSeen,
        currentDateSec,
        "First seen should remain unchanged"
      );
    }
  );

  doExperimentCleanup();
});

/**
 * This tests that the toggle does not change to the icon toggle if duration is negative.
 */
add_task(async function test_experiment_displayDuration_negative_duration() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      displayDuration: -1,
    },
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_FIRST_SEEN_PREF, 0],
          [TOGGLE_HAS_USED_PREF, false],
        ],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      const hasUsed = Services.prefs.getBoolPref(TOGGLE_HAS_USED_PREF);
      const firstSeen = Services.prefs.getIntPref(TOGGLE_FIRST_SEEN_PREF);

      Assert.ok(!hasUsed, "has-used is false and toggle is not icon");
      Assert.notEqual(firstSeen, 0, "First seen should not be 0");
    }
  );

  doExperimentCleanup();
});

/**
 * This tests that first-seen is only recorded for the first-time toggle.
 */
add_task(async function test_experiment_displayDuration_already_icon() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      displayDuration: 1,
    },
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_FIRST_SEEN_PREF, 0],
          [TOGGLE_HAS_USED_PREF, true],
        ],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      const firstSeen = Services.prefs.getIntPref(TOGGLE_FIRST_SEEN_PREF);
      Assert.equal(firstSeen, 0, "First seen should be 0");
    }
  );

  doExperimentCleanup();
});
