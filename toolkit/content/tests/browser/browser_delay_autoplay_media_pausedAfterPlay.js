const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_nonAutoplayAudio.html";

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

/**
 * When media starts in an unvisited tab, we would delay its playback and resume
 * media playback when the tab goes to foreground first time. There are two test
 * cases are used to check different situations.
 *
 * The first one is used to check if the delayed media has been paused before
 * the tab goes to foreground. Then, when the tab goes to foreground, media
 * should still be paused.
 *
 * The second one is used to check if the delayed media has been paused, but
 * it eventually was started again before the tab goes to foreground. Then, when
 * the tab goes to foreground, media should be resumed.
 */
add_task(async function testShouldNotResumePausedMedia() {
  info("- open new background tab and wait until it finishes loading -");
  const tab = BrowserTestUtils.addTab(window.gBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- play media and then immediately pause it -");
  await doPlayThenPauseOnMedia(tab);

  info("- show delay media playback icon on tab -");
  await waitForTabBlockEvent(tab, true);

  info("- selecting tab as foreground tab would resume the tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- resuming tab should dismiss delay autoplay icon -");
  await waitForTabBlockEvent(tab, false);

  info("- paused media should still be paused -");
  await checkAudioPauseState(tab, true /* should be paused */);

  info("- paused media won't generate tab playing icon -");
  await waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testShouldResumePlayedMedia() {
  info("- open new background tab and wait until it finishes loading -");
  const tab = BrowserTestUtils.addTab(window.gBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- play media, pause it, then play it again -");
  await doPlayPauseThenPlayOnMedia(tab);

  info("- show delay media playback icon on tab -");
  await waitForTabBlockEvent(tab, true);

  info("- select tab as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- resuming tab should dismiss delay autoplay icon -");
  await waitForTabBlockEvent(tab, false);

  info("- played media should still be played -");
  await checkAudioPauseState(tab, false /* should be played */);

  info("- played media would generate tab playing icon -");
  await waitForTabPlayingEvent(tab, true);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Helper functions.
 */
async function checkAudioPauseState(tab, expectedPaused) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [expectedPaused],
    expectedPaused => {
      const audio = content.document.getElementById("testAudio");
      if (!audio) {
        ok(false, "Can't get the audio element!");
      }

      is(audio.paused, expectedPaused, "The pause state of audio is corret.");
    }
  );
}

async function doPlayThenPauseOnMedia(tab) {
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const audio = content.document.getElementById("testAudio");
    if (!audio) {
      ok(false, "Can't get the audio element!");
    }

    audio.play();
    audio.pause();
  });
}

async function doPlayPauseThenPlayOnMedia(tab) {
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const audio = content.document.getElementById("testAudio");
    if (!audio) {
      ok(false, "Can't get the audio element!");
    }

    audio.play();
    audio.pause();
    audio.play();
  });
}
