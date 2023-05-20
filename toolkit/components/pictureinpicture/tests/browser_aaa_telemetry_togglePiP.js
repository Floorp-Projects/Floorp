/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getTelemetryToggleEnabled() {
  const scalarData = Services.telemetry.getSnapshotForScalars(
    "main",
    false
  ).parent;
  return scalarData["pictureinpicture.toggle_enabled"];
}

/**
 * Tests telemetry for user toggling on or off PiP.
 */
add_task(async () => {
  const TOGGLE_PIP_ENABLED_PREF =
    "media.videocontrols.picture-in-picture.video-toggle.enabled";

  await SpecialPowers.pushPrefEnv({
    set: [[TOGGLE_PIP_ENABLED_PREF, true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      let contextPiPDisable = document.getElementById(
        "context_HidePictureInPictureToggle"
      );
      contextPiPDisable.click();
      const enabled = Services.prefs.getBoolPref(
        TOGGLE_PIP_ENABLED_PREF,
        false
      );

      Assert.equal(enabled, false, "PiP is disabled.");

      await TestUtils.waitForCondition(() => {
        return getTelemetryToggleEnabled() === false;
      });

      Assert.equal(
        getTelemetryToggleEnabled(),
        false,
        "PiP is disabled according to Telemetry."
      );

      await SpecialPowers.pushPrefEnv({
        set: [[TOGGLE_PIP_ENABLED_PREF, true]],
      });

      await TestUtils.waitForCondition(() => {
        return getTelemetryToggleEnabled() === true;
      });

      Assert.equal(
        getTelemetryToggleEnabled(),
        true,
        "PiP is enabled according to Telemetry."
      );
    }
  );
});
