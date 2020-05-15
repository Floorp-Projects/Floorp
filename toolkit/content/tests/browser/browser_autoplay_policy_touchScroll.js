/**
 * This test is used to ensure that touch in point can activate document and
 * allow autoplay, but touch scroll can't activate document.
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_nonAutoplayAudio.html";

function checkMediaPlayingState(isPlaying) {
  let audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "can't get the audio element!");
  }

  is(!audio.paused, isPlaying, "media playing state is correct.");
}

async function callMediaPlay(shouldStartPlaying) {
  let audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "can't get the audio element!");
  }

  info(`call media.play().`);
  let playPromise = new Promise((resolve, reject) => {
    audio.play().then(() => {
      audio.isPlayStarted = true;
      resolve();
    });
    content.setTimeout(() => {
      if (audio.isPlayStarted) {
        return;
      }
      reject();
    }, 3000);
  });

  let isStartPlaying = await playPromise.then(
    () => true,
    () => false
  );
  is(
    isStartPlaying,
    shouldStartPlaying,
    "media is " + (isStartPlaying ? "" : "not ") + "playing."
  );
}

async function synthesizeTouchScroll(target, browser) {
  const offset = 100;
  await BrowserTestUtils.synthesizeTouch(
    target,
    0,
    0,
    { type: "touchstart" },
    browser
  );
  await BrowserTestUtils.synthesizeTouch(
    target,
    offset / 2,
    offset / 2,
    { type: "touchmove" },
    browser
  );
  await BrowserTestUtils.synthesizeTouch(
    target,
    offset,
    offset,
    { type: "touchend" },
    browser
  );
}

add_task(async function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.blocking_policy", 0],
    ],
  });
});

add_task(async function testTouchScroll() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async browser => {
      info(`- media should not start playing -`);
      await SpecialPowers.spawn(browser, [false], checkMediaPlayingState);

      info(`- simulate touch scroll which should not activate document -`);
      await synthesizeTouchScroll("#testAudio", browser);
      await SpecialPowers.spawn(browser, [false], callMediaPlay);

      info(`- simulate touch at a point which should activate document -`);
      await BrowserTestUtils.synthesizeTouch("#testAudio", 0, 0, {}, browser);
      await SpecialPowers.spawn(browser, [true], callMediaPlay);
    }
  );
});
