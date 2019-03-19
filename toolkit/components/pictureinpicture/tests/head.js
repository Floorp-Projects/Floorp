/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_PAGE = TEST_ROOT + "test-page.html";
const WINDOW_TYPE = "Toolkit:PictureInPicture";

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
 * @returns Promise
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
 * @returns Promise
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
