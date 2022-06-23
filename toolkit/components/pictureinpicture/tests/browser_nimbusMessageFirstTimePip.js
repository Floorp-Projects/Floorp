/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

const PIP_EXPERIMENT_MESSAGE = "Hello world message";
const PIP_EXPERIMENT_TITLE = "Hello world title";

/**
 * This function will hover over the middle of the video and then
 * hover over the toggle to reveal the first time PiP message
 * @param browser The current browser
 * @param videoID The video element id
 */
async function hoverToggle(browser, videoID) {
  await prepareForToggleClick(browser, videoID);

  // Hover the mouse over the video to reveal the toggle.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${videoID}`,
    {
      type: "mousemove",
    },
    browser
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${videoID}`,
    {
      type: "mouseover",
    },
    browser
  );

  info("Checking toggle policy");
  await assertTogglePolicy(browser, videoID, null);

  let toggleClientRect = await getToggleClientRect(browser, videoID);

  info("Hovering the toggle rect now.");
  let toggleCenterX = toggleClientRect.left + toggleClientRect.width / 2;
  let toggleCenterY = toggleClientRect.top + toggleClientRect.height / 2;

  await BrowserTestUtils.synthesizeMouseAtPoint(
    toggleCenterX,
    toggleCenterY,
    {
      type: "mousemove",
    },
    browser
  );
  await BrowserTestUtils.synthesizeMouseAtPoint(
    toggleCenterX,
    toggleCenterY,
    {
      type: "mouseover",
    },
    browser
  );
}

/**
 * This tests that the original DTD string is shown for the PiP toggle
 */
add_task(async function test_experiment_control() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const l10n = new Localization(
        ["branding/brand.ftl", "toolkit/global/videocontrols.ftl"],
        true
      );

      let pipExplainerMessage = l10n.formatValueSync(
        "videocontrols-picture-in-picture-explainer2"
      );

      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);

      const PIP_PREF =
        "media.videocontrols.picture-in-picture.video-toggle.has-used";
      await SpecialPowers.pushPrefEnv({
        set: [[PIP_PREF, false]],
      });

      let videoID = "with-controls";
      await hoverToggle(browser, videoID);

      await SpecialPowers.spawn(browser, [pipExplainerMessage], async function(
        pipExplainerMessage
      ) {
        let video = content.document.getElementById("with-controls");
        let shadowRoot = video.openOrClosedShadowRoot;
        let pipButton = shadowRoot.querySelector(".pip-explainer");

        Assert.equal(
          pipButton.textContent.trim(),
          pipExplainerMessage,
          "The PiP explainer is default"
        );
      });
    }
  );
});

/**
 * This tests that the experiment message is shown for the PiP toggle
 */
add_task(async function test_experiment_message() {
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      title: PIP_EXPERIMENT_TITLE,
      message: PIP_EXPERIMENT_MESSAGE,
    },
  });

  registerCleanupFunction(async function() {
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
        [PIP_EXPERIMENT_MESSAGE, PIP_EXPERIMENT_TITLE],
        async function(PIP_EXPERIMENT_MESSAGE, PIP_EXPERIMENT_TITLE) {
          let video = content.document.getElementById("with-controls");
          let shadowRoot = video.openOrClosedShadowRoot;
          let pipExplainer = shadowRoot.querySelector(".pip-explainer");
          let pipLabel = shadowRoot.querySelector(".pip-label");

          Assert.equal(
            pipExplainer.textContent.trim(),
            PIP_EXPERIMENT_MESSAGE,
            "The PiP explainer is being overridden by the experiment"
          );

          Assert.equal(
            pipLabel.textContent.trim(),
            PIP_EXPERIMENT_TITLE,
            "The PiP label is being overridden by the experiment"
          );
        }
      );
    }
  );
});
