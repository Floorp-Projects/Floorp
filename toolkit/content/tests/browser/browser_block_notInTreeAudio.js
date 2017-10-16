/* eslint-disable mozilla/no-arbitrary-setTimeout */
const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_nonAutoplayAudio.html";

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

function play_not_in_tree_audio() {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  content.document.body.removeChild(audio);

  // Add timeout to ensure the audio is removed from DOM tree.
  setTimeout(function() {
    info("Prepare to start playing audio.");
    audio.play();
  }, 1000);
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(async function block_not_in_tree_media() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- tab should not be blocked -");
  await waitForTabBlockEvent(tab, false);

  info("- check audio's suspend state -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- check audio's playing state -");
  await ContentTask.spawn(tab.linkedBrowser, true,
                          check_audio_pause_state);

  info("- playing audio -");
  await ContentTask.spawn(tab.linkedBrowser, null,
                          play_not_in_tree_audio);

  info("- tab should be blocked -");
  await waitForTabBlockEvent(tab, true);

  info("- switch tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- tab should be resumed -");
  await waitForTabBlockEvent(tab, false);

  info("- tab should be audible -");
  await waitForTabPlayingEvent(tab, true);

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
});
