/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const VIDEOS = [
  "with-controls",
  "no-controls",
];

/**
 * Tests that the Picture-in-Picture toggle is hidden when
 * a video with or without controls is made fullscreen.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_PAGE,
  }, async browser => {
    for (let videoID of VIDEOS) {
      let entered = BrowserTestUtils.waitForEvent(window, "MozDOMFullscreen:Entered");
      await ContentTask.spawn(browser, videoID, async videoID => {
        let video = this.content.document.getElementById(videoID);
        video.requestFullscreen();
      });
      await entered;

      await BrowserTestUtils.waitForCondition(() => {
        return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
      });

      await BrowserTestUtils.synthesizeMouseAtCenter(`#${videoID}`, {
        type: "mouseover",
      }, browser);

      let args = { videoID, TOGGLE_ID };
      let exited = BrowserTestUtils.waitForEvent(window, "MozDOMFullscreen:Exited");
      await ContentTask.spawn(browser, args, async args => {
        let { videoID, TOGGLE_ID } = args;
        let video = this.content.document.getElementById(videoID);
        let toggle = video.openOrClosedShadowRoot.getElementById(TOGGLE_ID);
        ok(ContentTaskUtils.is_hidden(toggle), "Toggle should be hidden in fullscreen mode.");
        this.content.document.exitFullscreen();
      });
      await exited;

      await BrowserTestUtils.waitForCondition(() => {
        return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
      });
    }
  });
});

/**
 * Tests that the Picture-in-Picture toggle is hidden if an
 * ancestor of a video (in this case, the document body) is made
 * to be the fullscreen element.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_PAGE,
  }, async browser => {
    let entered = BrowserTestUtils.waitForEvent(window, "MozDOMFullscreen:Entered");
    await ContentTask.spawn(browser, null, async () => {
      this.content.document.body.requestFullscreen();
    });
    await entered;

    await BrowserTestUtils.waitForCondition(() => {
      return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
    });

    for (let videoID of VIDEOS) {
      await BrowserTestUtils.synthesizeMouseAtCenter(`#${videoID}`, {
        type: "mouseover",
      }, browser);

      let args = { videoID, TOGGLE_ID };
      await ContentTask.spawn(browser, args, async args => {
        let { videoID, TOGGLE_ID } = args;
        let video = this.content.document.getElementById(videoID);
        let toggle = video.openOrClosedShadowRoot.getElementById(TOGGLE_ID);
        ok(ContentTaskUtils.is_hidden(toggle), "Toggle should be hidden in fullscreen mode.");
      });
    }

    let exited = BrowserTestUtils.waitForEvent(window, "MozDOMFullscreen:Exited");
    await ContentTask.spawn(browser, null, async () => {
      this.content.document.exitFullscreen();
    });
    await exited;

    await BrowserTestUtils.waitForCondition(() => {
      return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
    });
  });
});
