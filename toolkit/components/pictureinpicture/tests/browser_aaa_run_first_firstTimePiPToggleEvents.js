/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const FIRST_TIME_PIP_TOGGLE_STYLES = {
  rootID: "pictureInPictureToggle",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-wrapper": DEFAULT_TOGGLE_OPACITY,
      },
      hidden: [],
    },

    hoverToggle: {
      opacities: {
        ".pip-wrapper": 1.0,
      },
      hidden: [],
    },
  },
};

const FIRST_CONTEXT_MENU_EXPECTED_EVENTS = [
  [
    "pictureinpicture",
    "opened_method",
    "contextMenu",
    null,
    { firstTimeToggle: "true" },
  ],
];

const SECOND_CONTEXT_MENU_EXPECTED_EVENTS = [
  [
    "pictureinpicture",
    "opened_method",
    "contextMenu",
    null,
    { firstTimeToggle: "false" },
  ],
];

const FIRST_TOGGLE_EXPECTED_EVENTS = [
  ["pictureinpicture", "saw_toggle", "toggle", null, { firstTime: "true" }],
  [
    "pictureinpicture",
    "opened_method",
    "toggle",
    null,
    { firstTimeToggle: "true" },
  ],
];

const SECOND_TOGGLE_EXPECTED_EVENTS = [
  ["pictureinpicture", "saw_toggle", "toggle", null, { firstTime: "false" }],
  [
    "pictureinpicture",
    "opened_method",
    "toggle",
    null,
    { firstTimeToggle: "false" },
  ],
];

/**
 * This function will open the PiP window by clicking the toggle
 * and then close the PiP window
 * @param browser The current browser
 * @param videoID The video element id
 */
async function openAndClosePipWithToggle(browser, videoID) {
  await SimpleTest.promiseFocus(browser);
  await ensureVideosReady(browser);

  await prepareForToggleClick(browser, videoID);

  await clearAllContentEvents();

  // Hover the mouse over the video to reveal the toggle, which is necessary
  // if we want to click on the toggle.
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

  info("Waiting for toggle to become visible");
  await toggleOpacityReachesThreshold(
    browser,
    videoID,
    "hoverVideo",
    FIRST_TIME_PIP_TOGGLE_STYLES
  );

  let toggleClientRect = await getToggleClientRect(browser, videoID);

  // The toggle center, because of how it slides out, is actually outside
  // of the bounds of a click event. For now, we move the mouse in by a
  // hard-coded 15 pixels along the x and y axis to achieve the hover.
  let toggleLeft = toggleClientRect.left + 15;
  let toggleTop = toggleClientRect.top + 15;

  info("Clicking on toggle, and expecting a Picture-in-Picture window to open");
  // We need to wait for the window to have completed loading before we
  // can close it as the document's type required by closeWindow may not
  // be available.
  let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);

  await BrowserTestUtils.synthesizeMouseAtPoint(
    toggleLeft,
    toggleTop,
    {
      type: "mousedown",
    },
    browser
  );

  await BrowserTestUtils.synthesizeMouseAtPoint(
    1,
    1,
    {
      type: "mouseup",
    },
    browser
  );

  let win = await domWindowOpened;
  ok(win, "A Picture-in-Picture window opened.");

  await SpecialPowers.spawn(browser, [videoID], async videoID => {
    let video = content.document.getElementById(videoID);
    await ContentTaskUtils.waitForCondition(() => {
      return video.isCloningElementVisually;
    }, "Video is being cloned visually.");
  });

  await BrowserTestUtils.closeWindow(win);
  await assertSawClickEventOnly(browser);

  await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, browser);
  await assertSawMouseEvents(browser, true);
}

/**
 * This function will open the PiP window by with the context menu
 * @param browser The current browser
 * @param videoID The video element id
 */
async function openAndClosePipWithContextMenu(browser, videoID) {
  await SimpleTest.promiseFocus(browser);
  await ensureVideosReady(browser);

  let menu = document.getElementById("contentAreaContextMenu");
  let popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${videoID}`,
    {
      type: "contextmenu",
    },
    browser
  );

  await popupshown;
  let isContextMenuOpen = menu.state === "showing" || menu.state === "open";
  ok(isContextMenuOpen, "Context menu is open");

  let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);

  // clear content events
  await clearAllContentEvents();

  let hidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
  menu.activateItem(menu.querySelector("#context-video-pictureinpicture"));
  await hidden;

  let win = await domWindowOpened;
  ok(win, "A Picture-in-Picture window opened.");

  await SpecialPowers.spawn(browser, [videoID], async videoID => {
    let video = content.document.getElementById(videoID);
    await ContentTaskUtils.waitForCondition(() => {
      return video.isCloningElementVisually;
    }, "Video is being cloned visually.");
  });

  await BrowserTestUtils.closeWindow(win);
}

async function clearAllContentEvents() {
  // Clear everything.
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });
}

add_task(async function test_eventTelemetry() {
  Services.telemetry.clearEvents();
  await clearAllContentEvents();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      Services.telemetry.setEventRecordingEnabled("pictureinpicture", true);
      let videoID = "no-controls";

      const PIP_PREF =
        "media.videocontrols.picture-in-picture.video-toggle.has-used";
      await SpecialPowers.pushPrefEnv({
        set: [[PIP_PREF, false]],
      });

      // open with context menu for first time
      await openAndClosePipWithContextMenu(browser, videoID);

      let filter = {
        category: "pictureinpicture",
        method: "opened_method",
        object: "contextMenu",
      };
      await waitForTelemeryEvents(
        filter,
        FIRST_CONTEXT_MENU_EXPECTED_EVENTS.length,
        "content"
      );

      TelemetryTestUtils.assertEvents(
        FIRST_CONTEXT_MENU_EXPECTED_EVENTS,
        filter,
        { clear: true, process: "content" }
      );

      // open with toggle for first time
      await SpecialPowers.pushPrefEnv({
        set: [[PIP_PREF, false]],
      });

      await openAndClosePipWithToggle(browser, videoID);

      filter = {
        category: "pictureinpicture",
      };
      await waitForTelemeryEvents(
        filter,
        FIRST_TOGGLE_EXPECTED_EVENTS.length,
        "content"
      );

      TelemetryTestUtils.assertEvents(FIRST_TOGGLE_EXPECTED_EVENTS, filter, {
        clear: true,
        process: "content",
      });

      // open with toggle for not first time
      await openAndClosePipWithToggle(browser, videoID);

      filter = {
        category: "pictureinpicture",
      };
      await waitForTelemeryEvents(
        filter,
        SECOND_TOGGLE_EXPECTED_EVENTS.length,
        "content"
      );

      TelemetryTestUtils.assertEvents(SECOND_TOGGLE_EXPECTED_EVENTS, filter, {
        clear: true,
        process: "content",
      });

      // open with context menu for not first time
      await openAndClosePipWithContextMenu(browser, videoID);

      filter = {
        category: "pictureinpicture",
        method: "opened_method",
        object: "contextMenu",
      };
      await waitForTelemeryEvents(
        filter,
        SECOND_CONTEXT_MENU_EXPECTED_EVENTS.length,
        "content"
      );

      TelemetryTestUtils.assertEvents(
        SECOND_CONTEXT_MENU_EXPECTED_EVENTS,
        filter,
        { true: false, process: "content" }
      );
    }
  );
});
