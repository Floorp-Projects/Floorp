/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const FIRST_TIME_PIP_TOGGLE_STYLES = {
  rootID: "pictureInPictureToggle",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-wrapper": 0.8,
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
    "method",
    null,
    { method: "contextMenu", firstTimeToggle: "true" },
  ],
];

const SECOND_CONTEXT_MENU_EXPECTED_EVENTS = [
  [
    "pictureinpicture",
    "opened_method",
    "method",
    null,
    { method: "contextMenu", firstTimeToggle: "false" },
  ],
];

const FIRST_TOGGLE_EXPECTED_EVENTS = [
  ["pictureinpicture", "saw_toggle", "toggle", null, { firstTime: "true" }],
  [
    "pictureinpicture",
    "opened_method",
    "method",
    null,
    { method: "toggle", firstTimeToggle: "true" },
  ],
];

const SECOND_TOGGLE_EXPECTED_EVENTS = [
  ["pictureinpicture", "saw_toggle", "toggle", null, { firstTime: "false" }],
  [
    "pictureinpicture",
    "opened_method",
    "method",
    null,
    { method: "toggle", firstTimeToggle: "false" },
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
  await assertSawMouseEvents(browser, false);

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
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${videoID}`,
    {
      type: "contextmenu",
    },
    browser
  );

  await popupshown;
  let isContextMenuOpen = menu.state === "showing" || menu.state === "open";
  Assert.equal(isContextMenuOpen, true, "Context menu is open");

  let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);

  // clear content events
  await clearAllContentEvents();

  menu.querySelector("#context-video-pictureinpicture").click();

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

      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).content;

      info(`Length of events is ${events?.length}`);
      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).content;
          info(`Length of events is ${events?.length}`);
          return events && events.length >= 1;
        },
        "Waiting for one opened_method pictureinpicture telemetry event.",
        200,
        100
      );
      info(JSON.stringify(events));

      TelemetryTestUtils.assertEvents(
        FIRST_CONTEXT_MENU_EXPECTED_EVENTS,
        {
          category: "pictureinpicture",
          method: "opened_method",
          object: "method",
        },
        { clear: true, process: "content" }
      );

      // open with toggle for first time
      await SpecialPowers.pushPrefEnv({
        set: [[PIP_PREF, false]],
      });

      await openAndClosePipWithToggle(browser, videoID);

      events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).content;

      info(`Length of events is ${events?.length}`);
      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).content;
          info(`Length of events is ${events?.length}`);
          return events && events.length >= 2;
        },
        "Waiting for both pictureinpicture telemetry events.",
        200,
        100
      );
      info(JSON.stringify(events));

      TelemetryTestUtils.assertEvents(
        FIRST_TOGGLE_EXPECTED_EVENTS,
        { category: "pictureinpicture" },
        { clear: true, process: "content" }
      );

      // open with toggle for not first time
      await openAndClosePipWithToggle(browser, videoID);

      info(`Length of events is ${events?.length}`);
      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).content;
          info(`Length of events is ${events?.length}`);
          return events && events.length >= 2;
        },
        "Waiting for both pictureinpicture telemetry events.",
        200,
        100
      );
      info(JSON.stringify(events));

      TelemetryTestUtils.assertEvents(
        SECOND_TOGGLE_EXPECTED_EVENTS,
        { category: "pictureinpicture" },
        { clear: true, process: "content" }
      );

      // open with context menu for not first time
      await openAndClosePipWithContextMenu(browser, videoID);

      info(`Length of events is ${events?.length}`);
      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).content;
          info(`Length of events is ${events?.length}`);
          return events && events.length >= 1;
        },
        "Waiting for one opened_method pictureinpicture telemetry event.",
        200,
        100
      );
      info(JSON.stringify(events));

      TelemetryTestUtils.assertEvents(
        SECOND_CONTEXT_MENU_EXPECTED_EVENTS,
        {
          category: "pictureinpicture",
          method: "opened_method",
          object: "method",
        },
        { true: false, process: "content" }
      );
    }
  );
});
