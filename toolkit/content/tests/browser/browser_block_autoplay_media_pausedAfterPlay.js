const PAGE_SHOULD_PLAY = "https://example.com/browser/toolkit/content/tests/browser/file_blockMedia_shouldPlay.html";
const PAGE_SHOULD_NOT_PLAY = "https://example.com/browser/toolkit/content/tests/browser/file_blockMedia_shouldNotPlay.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3
};

function check_audio_suspended(suspendedType) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  is(audio.computedSuspended, suspendedType,
     "The suspeded state of audio is correct.");
}

function check_audio_pause_state(expectPause) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  is(audio.paused, expectPause,
    "The pause state of audio is corret.");
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(async function block_autoplay_media() {
  info("- open new background tab1, and check tab1's media suspend type -");
  let tab1 = window.gBrowser.addTab("about:blank");
  tab1.linkedBrowser.loadURI(PAGE_SHOULD_NOT_PLAY);
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);
  await ContentTask.spawn(tab1.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- the tab1 should not be blocked -");
  await waitForTabBlockEvent(tab1, false);

  info("- open new background tab2, and check tab2's media suspend type -");
  let tab2 = window.gBrowser.addTab("about:blank");
  tab2.linkedBrowser.loadURI(PAGE_SHOULD_PLAY);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  await ContentTask.spawn(tab2.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- the tab2 should be blocked -");
  await waitForTabBlockEvent(tab2, true);

  info("- select tab1 as foreground tab, and tab1's media should be paused -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab1);
  await ContentTask.spawn(tab1.linkedBrowser, true,
                          check_audio_pause_state);

  info("- the tab1 should not be blocked -");
  await waitForTabBlockEvent(tab1, false);

  info("- select tab2 as foreground tab, and the tab2 should not be blocked -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab2);
  await waitForTabBlockEvent(tab2, false);

  info("- tab2's media should be playing -");
  await waitForTabPlayingEvent(tab2, true);
  await ContentTask.spawn(tab2.linkedBrowser, false,
                          check_audio_pause_state);

  info("- check tab2's media suspend type -");
  await ContentTask.spawn(tab2.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- remove tabs -");
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});
