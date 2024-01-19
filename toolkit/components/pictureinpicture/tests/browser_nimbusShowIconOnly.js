/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const PIP_EXPERIMENT_MESSAGE = "Hello world message";
const PIP_EXPERIMENT_TITLE = "Hello world title";

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
        "videocontrols-picture-in-picture-explainer3"
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

      await SpecialPowers.spawn(
        browser,
        [pipExplainerMessage],
        async function (pipExplainerMessage) {
          let video = content.document.getElementById("with-controls");
          let shadowRoot = video.openOrClosedShadowRoot;
          let pipButton = shadowRoot.querySelector(".pip-explainer");

          Assert.equal(
            pipButton.textContent.trim(),
            pipExplainerMessage,
            "The PiP explainer is default"
          );
        }
      );
    }
  );
});

/**
 * This tests that the experiment is showing the icon only
 */
add_task(async function test_experiment_iconOnly() {
  let experimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "pictureinpicture",
    value: {
      showIconOnly: true,
    },
  });

  registerCleanupFunction(async function () {
    await experimentCleanup();
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

      await SpecialPowers.spawn(browser, [], async function () {
        let video = content.document.getElementById("with-controls");
        let shadowRoot = video.openOrClosedShadowRoot;
        let pipExpanded = shadowRoot.querySelector(".pip-expanded");
        let pipIcon = shadowRoot.querySelector("div.pip-icon");

        Assert.ok(
          ContentTaskUtils.isHidden(pipExpanded),
          "The PiP explainer hidden by the experiment"
        );

        Assert.ok(
          ContentTaskUtils.isVisible(pipIcon),
          "The PiP icon is visible by the experiment"
        );
      });
    }
  );
});
