/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_LONG = TEST_ROOT + "test-video-selection.html";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const EXPECTED_EVENT_CREATE = [
  [
    "pictureinpicture",
    "create",
    "player",
    undefined,
    { ccEnabled: "false", webVTTSubtitles: "false" },
  ],
];

const EXPECTED_EVENT_CREATE_WITH_TEXT_TRACKS = [
  [
    "pictureinpicture",
    "create",
    "player",
    undefined,
    { ccEnabled: "true", webVTTSubtitles: "true" },
  ],
];

const EXPECTED_EVENT_CLOSED_METHOD_CLOSE_BUTTON = [
  {
    category: "pictureinpicture",
    method: "closed_method",
    object: "closeButton",
  },
];

const videoID = "with-controls";

const EXPECTED_EVENT_CLOSED_METHOD_UNPIP = [
  {
    category: "pictureinpicture",
    method: "closed_method",
    object: "unpip",
  },
];

const FULLSCREEN_EVENTS = [
  {
    category: "pictureinpicture",
    method: "fullscreen",
    object: "player",
    extraKey: { enter: "true" },
  },
  {
    category: "pictureinpicture",
    method: "fullscreen",
    object: "player",
    extraKey: { enter: "true" },
  },
];

add_task(async function testCreateAndCloseButtonTelemetry() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      Services.telemetry.clearEvents();

      await ensureVideosReady(browser);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let filter = {
        category: "pictureinpicture",
        method: "create",
        object: "player",
      };
      await waitForTelemeryEvents(
        filter,
        EXPECTED_EVENT_CREATE.length,
        "parent"
      );

      TelemetryTestUtils.assertEvents(EXPECTED_EVENT_CREATE, filter, {
        clear: true,
        process: "parent",
      });

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;

      filter = {
        category: "pictureinpicture",
        method: "closed_method",
        object: "closeButton",
      };
      await waitForTelemeryEvents(
        filter,
        EXPECTED_EVENT_CLOSED_METHOD_CLOSE_BUTTON.length,
        "parent"
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CLOSED_METHOD_CLOSE_BUTTON,
        filter,
        { clear: true, process: "parent" }
      );

      let hist = TelemetryTestUtils.getAndClearHistogram(
        "FX_PICTURE_IN_PICTURE_WINDOW_OPEN_DURATION"
      );

      Assert.ok(hist, "Histogram exists");
    }
  );
});

add_task(async function textTextTracksAndUnpipTelemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        true,
      ],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();

      await ensureVideosReady(browser);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let filter = {
        category: "pictureinpicture",
        method: "create",
        object: "player",
      };
      await waitForTelemeryEvents(
        filter,
        EXPECTED_EVENT_CREATE_WITH_TEXT_TRACKS.length,
        "parent"
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CREATE_WITH_TEXT_TRACKS,
        filter,
        { clear: true, process: "parent" }
      );

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let unpipButton = pipWin.document.getElementById("unpip");
      EventUtils.synthesizeMouseAtCenter(unpipButton, {}, pipWin);
      await pipClosed;

      filter = {
        category: "pictureinpicture",
        method: "closed_method",
        object: "unpip",
      };
      await waitForTelemeryEvents(
        filter,
        EXPECTED_EVENT_CLOSED_METHOD_UNPIP.length,
        "parent"
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CLOSED_METHOD_UNPIP,
        filter,
        { clear: true, process: "parent" }
      );
    }
  );
});

add_task(async function test_fullscreen_events() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();

      await ensureVideosReady(browser);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let fullscreenBtn = pipWin.document.getElementById("fullscreen");

      await promiseFullscreenEntered(pipWin, () => {
        fullscreenBtn.click();
      });

      await promiseFullscreenExited(pipWin, () => {
        fullscreenBtn.click();
      });

      let filter = {
        category: "pictureinpicture",
        method: "fullscreen",
        object: "player",
      };
      await waitForTelemeryEvents(filter, FULLSCREEN_EVENTS.length, "parent");

      TelemetryTestUtils.assertEvents(FULLSCREEN_EVENTS, filter, {
        clear: true,
        process: "parent",
      });

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);
    }
  );
});
