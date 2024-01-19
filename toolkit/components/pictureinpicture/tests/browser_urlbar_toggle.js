/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const PIP_URLBAR_EVENTS = [
  {
    category: "pictureinpicture",
    method: "opened_method",
    object: "urlBar",
  },
];

const PIP_DISABLED_EVENTS = [
  {
    category: "pictureinpicture",
    method: "opened_method",
    object: "urlBar",
    extra: { disableDialog: "true" },
  },
  {
    category: "pictureinpicture",
    method: "disrespect_disable",
    object: "urlBar",
  },
];

const FIRST_TOGGLE_AFTER_CALLOUT_EXPECTED_EVENTS = [
  {
    category: "pictureinpicture",
    method: "opened_method",
    object: "urlBar",
    extra: { firstTimeToggle: "true", callout: "true" },
  },
];

add_task(async function test_urlbar_toggle_multiple_contexts() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_MULTIPLE_CONTEXTS,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();
      await ensureVideosReady(browser);
      await ensureVideosReady(browser.browsingContext.children[0]);

      await TestUtils.waitForCondition(
        () =>
          PictureInPicture.getEligiblePipVideoCount(browser).totalPipCount ===
          2,
        "Waiting for videos to register"
      );

      let { totalPipCount } =
        PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 2, "Total PiP count is 2");

      let pipUrlbarToggle = document.getElementById(
        "picture-in-picture-button"
      );
      ok(
        BrowserTestUtils.isHidden(pipUrlbarToggle),
        "PiP urlbar toggle is hidden because there is more than 1 video"
      );

      // Remove one video from page so urlbar toggle will show
      await SpecialPowers.spawn(browser, [], async () => {
        let video = content.document.getElementById("with-controls");
        video.remove();
      });

      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["hidden"] },
        () => BrowserTestUtils.isVisible(pipUrlbarToggle)
      );

      ok(
        BrowserTestUtils.isVisible(pipUrlbarToggle),
        "PiP urlbar toggle is visible"
      );

      ({ totalPipCount } = PictureInPicture.getEligiblePipVideoCount(browser));
      is(totalPipCount, 1, "Total PiP count is 1");

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      pipUrlbarToggle.click();
      let win = await domWindowOpened;
      ok(win, "A Picture-in-Picture window opened.");

      await assertVideoIsBeingCloned(
        browser.browsingContext.children[0],
        "video"
      );

      let filter = {
        category: "pictureinpicture",
        method: "opened_method",
        object: "urlBar",
      };
      await waitForTelemeryEvents(filter, PIP_URLBAR_EVENTS.length, "content");

      TelemetryTestUtils.assertEvents(PIP_URLBAR_EVENTS, filter, {
        clear: true,
        process: "content",
      });

      let domWindowClosed = BrowserTestUtils.domWindowClosed(win);
      pipUrlbarToggle.click();
      await domWindowClosed;

      await SpecialPowers.spawn(browser, [], async () => {
        let iframe = content.document.getElementById("iframe");
        iframe.remove();
      });

      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["hidden"] },
        () => BrowserTestUtils.isHidden(pipUrlbarToggle)
      );

      ok(
        BrowserTestUtils.isHidden(pipUrlbarToggle),
        "PiP urlbar toggle is hidden because there are no videos on the page"
      );

      ({ totalPipCount } = PictureInPicture.getEligiblePipVideoCount(browser));
      is(totalPipCount, 0, "Total PiP count is 0");
    }
  );
});

add_task(async function test_urlbar_toggle_switch_tabs() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_TRANSPARENT_NESTED_IFRAMES,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      await TestUtils.waitForCondition(
        () =>
          PictureInPicture.getEligiblePipVideoCount(browser).totalPipCount ===
          1,
        "Waiting for video to register"
      );

      let { totalPipCount } =
        PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 1, "Total PiP count is 1");

      let pipUrlbarToggle = document.getElementById(
        "picture-in-picture-button"
      );
      ok(
        BrowserTestUtils.isVisible(pipUrlbarToggle),
        "PiP urlbar toggle is visible because there is 1 video"
      );

      let pipActivePromise = BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["pipactive"] },
        () => pipUrlbarToggle.hasAttribute("pipactive")
      );

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      pipUrlbarToggle.click();
      let win = await domWindowOpened;
      ok(win, "A Picture-in-Picture window opened.");

      await assertVideoIsBeingCloned(browser, "video");

      await pipActivePromise;

      ok(
        pipUrlbarToggle.hasAttribute("pipactive"),
        "We are PiP'd in this tab so the icon is active"
      );

      let newTab = BrowserTestUtils.addTab(
        gBrowser,
        TEST_PAGE_TRANSPARENT_NESTED_IFRAMES
      );
      await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);

      await BrowserTestUtils.switchTab(gBrowser, newTab);

      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["pipactive"] },
        () => !pipUrlbarToggle.hasAttribute("pipactive")
      );

      ok(
        !pipUrlbarToggle.hasAttribute("pipactive"),
        "After switching tabs the pip icon is not active"
      );

      BrowserTestUtils.removeTab(newTab);

      await ensureMessageAndClosePiP(
        browser,
        "video-transparent-background",
        win,
        false
      );
    }
  );
});

add_task(async function test_pipDisabled() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_PIP_DISABLED,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.respect-disablePictureInPicture",
            true,
          ],
        ],
      });

      const VIDEO_ID = "with-controls";
      await ensureVideosReady(browser);

      await TestUtils.waitForCondition(
        () =>
          PictureInPicture.getEligiblePipVideoCount(browser).totalPipCount ===
          1,
        "Waiting for video to register"
      );

      let { totalPipCount, totalPipDisabled } =
        PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 1, "Total PiP count is 1");
      is(totalPipDisabled, 1, "PiP is disabled on 1 video");

      // Confirm that the toggle is hidden because we are respecting disablePictureInPicture
      await testToggleHelper(browser, VIDEO_ID, false);

      let pipUrlbarToggle = document.getElementById(
        "picture-in-picture-button"
      );
      ok(
        BrowserTestUtils.isVisible(pipUrlbarToggle),
        "PiP urlbar toggle is visible because PiP is disabled"
      );

      pipUrlbarToggle.click();

      let panel = browser.ownerDocument.querySelector("#PictureInPicturePanel");
      await BrowserTestUtils.waitForCondition(async () => {
        if (!panel) {
          panel = browser.ownerDocument.querySelector("#PictureInPicturePanel");
        }
        return BrowserTestUtils.isVisible(panel);
      });

      let respectPipDisableSwitch = panel.querySelector(
        "#respect-pipDisabled-switch"
      );
      ok(
        !respectPipDisableSwitch.pressed,
        "Respect PiP disabled is not pressed"
      );

      EventUtils.synthesizeMouseAtCenter(respectPipDisableSwitch.buttonEl, {});
      await BrowserTestUtils.waitForEvent(respectPipDisableSwitch, "toggle");
      ok(respectPipDisableSwitch.pressed, "Respect PiP disabled is pressed");

      pipUrlbarToggle.click();

      await BrowserTestUtils.waitForCondition(async () => {
        return BrowserTestUtils.isHidden(panel);
      });

      let filter = {
        category: "pictureinpicture",
        object: "urlBar",
      };
      await waitForTelemeryEvents(filter, PIP_DISABLED_EVENTS.length, "parent");
      TelemetryTestUtils.assertEvents(PIP_DISABLED_EVENTS, filter, {
        clear: true,
        process: "parent",
      });

      // Confirm that the toggle is now visible because we no longer respect disablePictureInPicture
      await testToggleHelper(browser, VIDEO_ID, true);

      let pipActivePromise = BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["pipactive"] },
        () => pipUrlbarToggle.hasAttribute("pipactive")
      );

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      pipUrlbarToggle.click();
      let win = await domWindowOpened;
      ok(win, "A Picture-in-Picture window opened.");

      await assertVideoIsBeingCloned(browser, "video");

      await pipActivePromise;

      ok(
        pipUrlbarToggle.hasAttribute("pipactive"),
        "We are PiP'd in this tab so the icon is active"
      );

      let domWindowClosed = BrowserTestUtils.domWindowClosed(win);
      pipUrlbarToggle.click();
      await domWindowClosed;

      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["pipactive"] },
        () => !pipUrlbarToggle.hasAttribute("pipactive")
      );

      ok(
        !pipUrlbarToggle.hasAttribute("pipactive"),
        "We closed the PiP window so the urlbar button is no longer active"
      );

      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_urlbar_toggle_telemetry() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_MULTIPLE_CONTEXTS,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();
      await ensureVideosReady(browser);
      await ensureVideosReady(browser.browsingContext.children[0]);

      await TestUtils.waitForCondition(
        () =>
          PictureInPicture.getEligiblePipVideoCount(browser).totalPipCount ===
          2,
        "Waiting for videos to register"
      );

      let { totalPipCount } =
        PictureInPicture.getEligiblePipVideoCount(browser);
      is(totalPipCount, 2, "Total PiP count is 2");

      let pipUrlbarToggle = document.getElementById(
        "picture-in-picture-button"
      );
      ok(
        BrowserTestUtils.isHidden(pipUrlbarToggle),
        "PiP urlbar toggle is hidden because there is more than 1 video"
      );

      // Remove one video from page so urlbar toggle will show
      await SpecialPowers.spawn(browser, [], async () => {
        let video = content.document.getElementById("with-controls");
        video.remove();
      });
      await BrowserTestUtils.waitForMutationCondition(
        pipUrlbarToggle,
        { attributeFilter: ["hidden"] },
        () => BrowserTestUtils.isVisible(pipUrlbarToggle)
      );
      ok(
        BrowserTestUtils.isVisible(pipUrlbarToggle),
        "PiP urlbar toggle is visible"
      );

      // open for first time after seeing feature callout message
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.video-toggle.has-used",
            false,
          ],
        ],
      });
      let fakeMessage = {
        template: "feature_callout",
        id: "FAKE_PIP_FEATURE_CALLOUT",
        content: {
          screens: [{ anchors: [{ selector: "#picture-in-picture-button" }] }],
        },
      };
      await ASRouter.waitForInitialized;
      let originalASRouterState;
      await ASRouter.setState(state => {
        originalASRouterState = state;
        return {
          messages: [...state.messages, fakeMessage],
          messageImpressions: {
            ...state.messageImpressions,
            [fakeMessage.id]: [Date.now()],
          },
        };
      });

      ({ totalPipCount } = PictureInPicture.getEligiblePipVideoCount(browser));
      is(totalPipCount, 1, "Total PiP count is 1");

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      pipUrlbarToggle.click();
      let win = await domWindowOpened;
      ok(win, "A Picture-in-Picture window opened.");
      await assertVideoIsBeingCloned(
        browser.browsingContext.children[0],
        "video"
      );

      let filter = {
        category: "pictureinpicture",
        method: "opened_method",
        object: "urlBar",
      };
      await waitForTelemeryEvents(
        filter,
        FIRST_TOGGLE_AFTER_CALLOUT_EXPECTED_EVENTS.length,
        "content"
      );

      TelemetryTestUtils.assertEvents(
        FIRST_TOGGLE_AFTER_CALLOUT_EXPECTED_EVENTS,
        filter,
        { clear: true, process: "content" }
      );

      let domWindowClosed = BrowserTestUtils.domWindowClosed(win);
      pipUrlbarToggle.click();
      await domWindowClosed;

      await ASRouter.setState(originalASRouterState);
    }
  );
});
