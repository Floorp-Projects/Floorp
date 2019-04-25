/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_PAGE = TEST_ROOT + "test-page.html";
const WINDOW_TYPE = "Toolkit:PictureInPicture";
const TOGGLE_ID = "pictureInPictureToggleButton";
const HOVER_VIDEO_OPACITY = 0.8;
const HOVER_TOGGLE_OPACITY = 1.0;

/**
 * Given a browser and the ID for a <video> element, triggers
 * Picture-in-Picture for that <video>, and resolves with the
 * Picture-in-Picture window once it is ready to be used.
 *
 * @param {Element} browser The <xul:browser> hosting the <video>
 *
 * @param {String} videoID The ID of the video to trigger
 * Picture-in-Picture on.
 *
 * @return Promise
 * @resolves With the Picture-in-Picture window when ready.
 */
async function triggerPictureInPicture(browser, videoID) {
  let domWindowOpened = BrowserTestUtils.domWindowOpened(null);
  let videoReady = ContentTask.spawn(browser, videoID, async videoID => {
    let video = content.document.getElementById(videoID);
    let event = new content.CustomEvent("MozTogglePictureInPicture", { bubbles: true });
    video.dispatchEvent(event);
    await ContentTaskUtils.waitForCondition(() => {
      return video.isCloningElementVisually;
    }, "Video is being cloned visually.");
  });
  let win = await domWindowOpened;
  await BrowserTestUtils.waitForEvent(win, "load");
  await videoReady;
  return win;
}

/**
 * Given a browser and the ID for a <video> element, checks that the
 * video is showing the "This video is playing in Picture-in-Picture mode."
 * status message overlay.
 *
 * @param {Element} browser The <xul:browser> hosting the <video>
 *
 * @param {String} videoID The ID of the video to trigger
 * Picture-in-Picture on.
 *
 * @param {bool} expected True if we expect the message to be showing.
 *
 * @return Promise
 * @resolves When the checks have completed.
 */
async function assertShowingMessage(browser, videoID, expected) {
  let showing = await ContentTask.spawn(browser, videoID, async videoID => {
    let video = content.document.getElementById(videoID);
    let shadowRoot = video.openOrClosedShadowRoot;
    let pipOverlay = shadowRoot.querySelector(".pictureInPictureOverlay");
    ok(pipOverlay, "Should be able to find Picture-in-Picture overlay.");

    let rect = pipOverlay.getBoundingClientRect();
    return rect.height > 0 && rect.width > 0;
  });
  Assert.equal(showing, expected,
               "Video should be showing the expected state.");
}

async function toggleOpacityReachesThreshold(browser, videoID, opacityThreshold) {
  let args = { videoID, TOGGLE_ID, opacityThreshold };
  await ContentTask.spawn(browser, args, async args => {
    let { videoID, TOGGLE_ID, opacityThreshold } = args;
    let video = content.document.getElementById(videoID);
    let shadowRoot = video.openOrClosedShadowRoot;
    let toggle = shadowRoot.getElementById(TOGGLE_ID);

    await ContentTaskUtils.waitForCondition(() => {
      let opacity = parseFloat(this.content.getComputedStyle(toggle).opacity);
      return opacity >= opacityThreshold;
    }, `Toggle should have opacity >= ${opacityThreshold}`, 100, 100);

    ok(true, "Toggle reached target opacity.");
  });
}

/**
 * Test helper for the Picture-in-Picture toggle. Loads a page, and then
 * tests the provided video elements for the toggle both appearing and
 * opening the Picture-in-Picture window in the expected cases.
 *
 * @param {String} testURL The URL of the page with the <video> elements.
 * @param {Object} expectations An object with the following schema:
 *   <video-element-id>: {
 *     canToggle: Boolean
 *   }
 * If canToggle is true, then it's expected that moving the mouse over the
 * video and then clicking in the toggle region should open a
 * Picture-in-Picture window. If canToggle is false, we expect that a click
 * in this region will not result in the window opening.
 *
 * @return Promise
 * @resolves When the test is complete and the tab with the loaded page is
 * removed.
 */
async function testToggle(testURL, expectations) {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: testURL,
  }, async browser => {
    let videoIDs = Object.keys(expectations);

    // PictureInPictureToggleChild waits for videos to fire their "canplay"
    // event before considering them for the toggle, so we start by making
    // sure each <video> has done this.
    info(`Waiting for videos to be ready`);
    await ContentTask.spawn(browser, videoIDs, async videoIDs => {
      for (let videoID of videoIDs) {
        let video = content.document.getElementById(videoID);
        if (video.readyState < content.HTMLMediaElement.HAVE_ENOUGH_DATA) {
          await ContentTaskUtils.waitForEvent(video, "canplay");
        }
      }
    });

    for (let videoID of videoIDs) {
      await SimpleTest.promiseFocus(browser);
      info(`Testing video with id: ${videoID}`);

      // For each video, make sure it's scrolled into view, and get the rect for
      // the toggle while we're at it.
      let args = { videoID, TOGGLE_ID };
      let toggleClientRect = await ContentTask.spawn(browser, args, async args => {
        let { videoID, TOGGLE_ID } = args;
        let video = content.document.getElementById(videoID);
        video.scrollIntoView({ behaviour: "instant" });
        let shadowRoot = video.openOrClosedShadowRoot;
        let toggle = shadowRoot.getElementById(TOGGLE_ID);

        if (!video.controls) {
          // For no-controls <video> elements, an IntersectionObserver is used
          // to know when we the PictureInPictureChild should begin tracking
          // mousemove events. We don't exactly know when that IntersectionObserver
          // will fire, so we poll a special testing function that will tell us when
          // the video that we care about is being tracked.
          let {PictureInPictureToggleChild} =
            ChromeUtils.import("resource://gre/actors/PictureInPictureChild.jsm");
          await ContentTaskUtils.waitForCondition(() => {
            return PictureInPictureToggleChild.isTracking(video);
          }, "Waiting for PictureInPictureToggleChild to be tracking the video.", 100, 100);
        }
        let rect = toggle.getBoundingClientRect();
        return { top: rect.top, right: rect.right, left: rect.left, bottom: rect.bottom };
      });

      // Hover the mouse over the video to reveal the toggle.
      await BrowserTestUtils.synthesizeMouseAtCenter(`#${videoID}`, {
        type: "mousemove",
      }, browser);
      await BrowserTestUtils.synthesizeMouseAtCenter(`#${videoID}`, {
        type: "mouseover",
      }, browser);

      info("Waiting for toggle to become visible");
      await toggleOpacityReachesThreshold(browser, videoID, HOVER_VIDEO_OPACITY);

      info("Hovering the toggle rect now.");
      let toggleLeft = toggleClientRect.left + 2;
      let toggleTop = toggleClientRect.top + 2;
      await BrowserTestUtils.synthesizeMouseAtPoint(toggleLeft, toggleTop, {
        type: "mousemove",
      }, browser);
      await BrowserTestUtils.synthesizeMouseAtPoint(toggleLeft, toggleTop, {
        type: "mouseover",
      }, browser);

      await toggleOpacityReachesThreshold(browser, videoID, HOVER_TOGGLE_OPACITY);

      if (expectations[videoID].canToggle) {
        info("Clicking on toggle, and expecting a Picture-in-Picture window to open");
        let domWindowOpened = BrowserTestUtils.domWindowOpened(null);
        await BrowserTestUtils.synthesizeMouseAtPoint(toggleLeft, toggleTop, {}, browser);
        let win = await domWindowOpened;
        ok(win, "A Picture-in-Picture window opened.");
        await BrowserTestUtils.closeWindow(win);
      } else {
        info("Clicking on toggle, and expecting no Picture-in-Picture window opens");
        await BrowserTestUtils.synthesizeMouseAtPoint(toggleLeft, toggleTop, {}, browser);

        // The message to open the Picture-in-Picture window would normally be sent
        // immediately before this Promise resolved, so the window should have opened
        // by now if it was going to happen.
        for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
          if (!win.closed) {
            ok(false, "Found a Picture-in-Picture window unexpectedly.");
            return;
          }
        }

        ok(true, "No Picture-in-Picture window found.");
      }
    }
  });
}
