/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const EXPERIMENT_CLASS_NAME = "experiment";

/**
 * This tests that the original PiP toggle design is shown.
 */
add_task(async function test_experiment_control_toggle_style() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      const PIP_PREF =
        "media.videocontrols.picture-in-picture.video-toggle.has-used";
      await SpecialPowers.pushPrefEnv({
        set: [[PIP_PREF, false]],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      await SpecialPowers.spawn(
        browser,
        [EXPERIMENT_CLASS_NAME],
        async EXPERIMENT_CLASS_NAME => {
          let video = content.document.getElementById("with-controls");
          let shadowRoot = video.openOrClosedShadowRoot;

          let controlsContainer =
            shadowRoot.querySelector(".controlsContainer");
          let pipWrapper = shadowRoot.querySelector(".pip-wrapper");
          let pipExplainer = shadowRoot.querySelector(".pip-explainer");

          Assert.ok(
            !controlsContainer.classList.contains(EXPERIMENT_CLASS_NAME)
          );
          Assert.ok(!pipWrapper.classList.contains(EXPERIMENT_CLASS_NAME));
          Assert.ok(
            ContentTaskUtils.isVisible(pipExplainer),
            "The PiP message should be visible on the toggle"
          );
        }
      );
    }
  );
});

/**
 * This tests that the variant PiP toggle design is shown if Nimbus
 * variable `oldToggle` is false.
 */
add_task(async function test_experiment_toggle_style() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      oldToggle: false,
    },
  });

  registerCleanupFunction(async function () {
    await doExperimentCleanup();
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      const PIP_PREF =
        "media.videocontrols.picture-in-picture.video-toggle.has-used";
      await SpecialPowers.pushPrefEnv({
        set: [[PIP_PREF, false]],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      await SpecialPowers.spawn(
        browser,
        [EXPERIMENT_CLASS_NAME],
        async EXPERIMENT_CLASS_NAME => {
          let video = content.document.getElementById("with-controls");
          let shadowRoot = video.openOrClosedShadowRoot;

          let controlsContainer =
            shadowRoot.querySelector(".controlsContainer");
          let pipWrapper = shadowRoot.querySelector(".pip-wrapper");
          let pipExplainer = shadowRoot.querySelector(".pip-explainer");

          Assert.ok(
            controlsContainer.classList.contains(EXPERIMENT_CLASS_NAME)
          );
          Assert.ok(pipWrapper.classList.contains(EXPERIMENT_CLASS_NAME));
          Assert.ok(
            ContentTaskUtils.isHidden(pipExplainer),
            "The PiP message should not be visible on the toggle"
          );
        }
      );
    }
  );
});
