/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const videoID = "with-controls";
const TEXT_TRACK_FONT_SIZE =
  "media.videocontrols.picture-in-picture.display-text-tracks.size";
const ACCEPTABLE_DIFF = 1;

const checkFontSize = (actual, expected, str) => {
  let fs1 = actual.substring(0, actual.length - 2);
  let fs2 = expected;
  let diff = Math.abs(fs1 - fs2);
  info(`Actual font size: ${fs1}. Expected font size: ${fs2}`);
  Assert.lessOrEqual(diff, ACCEPTABLE_DIFF, str);
};

function getFontSize(pipBrowser) {
  return SpecialPowers.spawn(pipBrowser, [], async () => {
    info("Checking text track content in pip window");
    let textTracks = content.document.getElementById("texttracks");
    ok(textTracks, "TextTracks container should exist in the pip window");
    await ContentTaskUtils.waitForCondition(() => {
      return textTracks.textContent;
    }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
    return content.window.getComputedStyle(textTracks).fontSize;
  });
}

function promiseResize(win, width, height) {
  if (win.outerWidth == width && win.outerHeight == height) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    // More than one "resize" might be received if the window was recently
    // created.
    win.addEventListener("resize", () => {
      if (win.outerWidth == width && win.outerHeight == height) {
        resolve();
      }
    });
    win.resizeTo(width, height);
  });
}

/**
 * Tests the font size is correct for PiP windows
 */
add_task(async () => {
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
      gBrowser,
      url: TEST_PAGE_WITH_WEBVTT,
    },
    async browser => {
      await prepareVideosAndWebVTTTracks(browser, videoID);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // move PiP window to 0, 0 so resizing the window doesn't go offscreen
      pipWin.moveTo(0, 0);

      let width = pipWin.innerWidth;
      let height = pipWin.innerHeight;

      await promiseResize(pipWin, Math.round(250 * (width / height)), 250);

      width = pipWin.innerWidth;
      height = pipWin.innerHeight;

      let pipBrowser = pipWin.document.getElementById("browser");

      let fontSize = await getFontSize(pipBrowser);
      checkFontSize(
        fontSize,
        Math.round(height * 0.06 * 10) / 10,
        "The medium font size is .06 of the PiP window height"
      );

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);

      // change font size to small
      await SpecialPowers.pushPrefEnv({
        set: [[TEXT_TRACK_FONT_SIZE, "small"]],
      });

      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      pipBrowser = pipWin.document.getElementById("browser");

      fontSize = await getFontSize(pipBrowser);
      checkFontSize(fontSize, 14, "The small font size is the minimum 14px");

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);

      // change font size to large
      await SpecialPowers.pushPrefEnv({
        set: [[TEXT_TRACK_FONT_SIZE, "large"]],
      });

      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      pipBrowser = pipWin.document.getElementById("browser");

      fontSize = await getFontSize(pipBrowser);
      checkFontSize(
        fontSize,
        Math.round(height * 0.09 * 10) / 10,
        "The large font size is .09 of the PiP window height"
      );

      // resize PiP window to a larger size
      width = pipWin.innerWidth * 2;
      height = pipWin.innerHeight * 2;
      await promiseResize(pipWin, width, height);

      fontSize = await getFontSize(pipBrowser);
      checkFontSize(fontSize, 40, "The large font size is the max of 40px");

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);

      // change font size to small
      await SpecialPowers.pushPrefEnv({
        set: [[TEXT_TRACK_FONT_SIZE, "small"]],
      });

      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      pipBrowser = pipWin.document.getElementById("browser");

      fontSize = await getFontSize(pipBrowser);
      checkFontSize(
        fontSize,
        Math.round(height * 0.03 * 10) / 10,
        "The small font size is .03 of the PiP window height"
      );
    }
  );
});
