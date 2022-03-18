/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
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
  [
    "pictureinpicture",
    "closed_method",
    "method",
    null,
    { reason: "close-button" },
  ],
];

const videoID = "with-controls";

const EXPECTED_EVENT_CLOSED_METHOD_UNPIP = [
  ["pictureinpicture", "closed_method", "method", null, { reason: "unpip" }],
];

add_task(async () => {
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

      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;

      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).parent;
          return events && events.length >= 1;
        },
        "Waiting for one create pictureinpicture telemetry event.",
        200,
        100
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CREATE,
        {
          category: "pictureinpicture",
          method: "create",
          object: "player",
        },
        { clear: true, process: "parent" }
      );

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;

      events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;

      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).parent;
          return events && events.length >= 1;
        },
        "Waiting for one closed_method pictureinpicture telemetry event.",
        200,
        100
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CLOSED_METHOD_CLOSE_BUTTON,
        {
          category: "pictureinpicture",
          method: "closed_method",
          object: "method",
        },
        { clear: true, process: "parent" }
      );

      let hist = TelemetryTestUtils.getAndClearHistogram(
        "FX_PICTURE_IN_PICTURE_WINDOW_OPEN_DURATION"
      );

      Assert.ok(hist, "Histogram exists");
    }
  );
});

add_task(async function() {
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

      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;

      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).parent;
          return events && events.length >= 1;
        },
        "Waiting for one create pictureinpicture telemetry event.",
        200,
        100
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CREATE_WITH_TEXT_TRACKS,
        {
          category: "pictureinpicture",
          method: "create",
          object: "player",
        },
        { clear: true, process: "parent" }
      );

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let unpipButton = pipWin.document.getElementById("unpip");
      EventUtils.synthesizeMouseAtCenter(unpipButton, {}, pipWin);
      await pipClosed;

      events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;

      await TestUtils.waitForCondition(
        () => {
          events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).parent;
          return events && events.length >= 1;
        },
        "Waiting for one closed_method pictureinpicture telemetry event.",
        200,
        100
      );

      TelemetryTestUtils.assertEvents(
        EXPECTED_EVENT_CLOSED_METHOD_UNPIP,
        {
          category: "pictureinpicture",
          method: "closed_method",
          object: "method",
        },
        { clear: true, process: "parent" }
      );
    }
  );
});
