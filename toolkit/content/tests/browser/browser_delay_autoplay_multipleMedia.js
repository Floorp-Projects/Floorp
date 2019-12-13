/* eslint-disable mozilla/no-arbitrary-setTimeout */
const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

function check_autoplay_audio_onplay() {
  let autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  return new Promise((resolve, reject) => {
    autoPlay.onplay = () => {
      ok(false, "Should not receive play event!");
      this.onplay = null;
      reject();
    };

    autoPlay.pause();
    autoPlay.play();

    content.setTimeout(() => {
      ok(true, "Doesn't receive play event when media was blocked.");
      autoPlay.onplay = null;
      resolve();
    }, 1000);
  });
}

function play_nonautoplay_audio_should_be_blocked() {
  let nonAutoPlay = content.document.getElementById("nonautoplay");
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  nonAutoPlay.play();
  ok(nonAutoPlay.paused, "The blocked audio can't be playback.");
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

add_task(async function block_multiple_media() {
  info("- open new background tab -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  BrowserTestUtils.loadURI(browser, PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  info("- tab should be blocked -");
  await waitForTabBlockEvent(tab, true);

  info("- autoplay media should be blocked -");
  await SpecialPowers.spawn(browser, [], check_autoplay_audio_onplay);

  info("- non-autoplay can't start playback when the tab is blocked -");
  await SpecialPowers.spawn(
    browser,
    [],
    play_nonautoplay_audio_should_be_blocked
  );

  info("- select tab as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- tab should be resumed -");
  await waitForTabPlayingEvent(tab, true);
  await waitForTabBlockEvent(tab, false);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
