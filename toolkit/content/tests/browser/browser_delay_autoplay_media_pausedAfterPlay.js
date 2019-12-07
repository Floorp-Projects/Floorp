const PAGE_SHOULD_PLAY =
  "https://example.com/browser/toolkit/content/tests/browser/file_blockMedia_shouldPlay.html";
const PAGE_SHOULD_NOT_PLAY =
  "https://example.com/browser/toolkit/content/tests/browser/file_blockMedia_shouldNotPlay.html";

function check_audio_pause_state(expectPause) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  is(audio.paused, expectPause, "The pause state of audio is corret.");
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

add_task(async function block_autoplay_media() {
  info("- open new background tab1, and tab1 should not be blocked -");
  let tab1 = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  BrowserTestUtils.loadURI(tab1.linkedBrowser, PAGE_SHOULD_NOT_PLAY);
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);
  await waitForTabBlockEvent(tab1, false);

  info("- open new background tab2, and tab2 should be blocked -");
  let tab2 = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  BrowserTestUtils.loadURI(tab2.linkedBrowser, PAGE_SHOULD_PLAY);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  await waitForTabBlockEvent(tab2, true);

  info("- select tab1 as foreground tab  -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab1);

  info("- tab1 should not be blocked, and media should not started -");
  await waitForTabBlockEvent(tab1, false);
  await ContentTask.spawn(tab1.linkedBrowser, true, check_audio_pause_state);

  info("- select tab2 as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab2);

  info("- tab2 should not be blocked and media should be playing -");
  await waitForTabBlockEvent(tab2, false);
  await waitForTabPlayingEvent(tab2, true);
  await ContentTask.spawn(tab2.linkedBrowser, false, check_audio_pause_state);

  info("- remove tabs -");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
