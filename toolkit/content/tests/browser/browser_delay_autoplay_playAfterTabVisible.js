const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_nonAutoplayAudio.html";

function check_audio_pause_state(expectPause) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  is(audio.paused, expectPause, "The pause state of audio is corret.");
}

function play_audio() {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  audio.play();
  return new Promise(resolve => {
    audio.onplay = function() {
      audio.onplay = null;
      ok(true, "Audio starts playing.");
      resolve();
    };
  });
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

/**
 * This test is used for testing the visible tab which was not resumed yet.
 * If the tab doesn't have any media component, it won't be resumed even it
 * has already gone to foreground until we start audio.
 */
add_task(async function media_should_be_able_to_play_in_visible_tab() {
  info("- open new background tab, and check tab's media pause state -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [true], check_audio_pause_state);

  info(
    "- select tab as foreground tab, and tab's media should still be paused -"
  );
  await BrowserTestUtils.switchTab(window.gBrowser, tab);
  await SpecialPowers.spawn(tab.linkedBrowser, [true], check_audio_pause_state);

  info("- start audio in tab -");
  await SpecialPowers.spawn(tab.linkedBrowser, [], play_audio);

  info("- audio should be playing -");
  await waitForTabBlockEvent(tab, false);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [false],
    check_audio_pause_state
  );

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
