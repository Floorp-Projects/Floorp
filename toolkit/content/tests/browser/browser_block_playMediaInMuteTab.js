const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_nonAutoplayAudio.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3
};

function wait_for_event(browser, event) {
  return BrowserTestUtils.waitForEvent(browser, event, false, (event) => {
    is(event.originalTarget, browser, "Event must be dispatched to correct browser.");
    return true;
  });
}

function check_audio_volume_and_mute(expectedMute) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  let expectedVolume = expectedMute ? 0.0 : 1.0;
  is(expectedVolume, audio.computedVolume, "Audio's volume is correct!");
  is(expectedMute, audio.computedMuted, "Audio's mute state is correct!");
}

function check_audio_suspended(suspendedType) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  is(audio.computedSuspended, suspendedType,
     "The suspeded state of audio is correct.");
}

function check_audio_pause_state(expectedPauseState) {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  is(audio.paused, expectedPauseState, "Audio is paused.");
}

function play_audio() {
  var audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
  }

  audio.play();
  ok(audio.paused, "Can't play audio, because the tab was still blocked.");
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(async function unblock_icon_should_disapear_after_resume_tab() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- audio doesn't be started in beginning -");
  await ContentTask.spawn(tab.linkedBrowser, true,
                          check_audio_pause_state);

  info("- audio shouldn't be muted or blocked -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);
  await ContentTask.spawn(tab.linkedBrowser, false /* unmute */,
                          check_audio_volume_and_mute);

  info("- tab shouldn't display unblocking icon -");
  await waitForTabBlockEvent(tab, false);

  info("- mute tab -");
  tab.linkedBrowser.mute();
  ok(tab.linkedBrowser.audioMuted, "Audio should be muted now");

  info("- try to start audio in background tab -");
  await ContentTask.spawn(tab.linkedBrowser, null,
                          play_audio);

  info("- audio should be muted and blocked -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);
  await ContentTask.spawn(tab.linkedBrowser, true /* mute */,
                          check_audio_volume_and_mute);

  info("- tab should display unblocking icon -");
  await waitForTabBlockEvent(tab, true);

  info("- select tab as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- audio shoule be muted, but not be blocked -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);
  await ContentTask.spawn(tab.linkedBrowser, true /* mute */,
                          check_audio_volume_and_mute);

  info("- tab should not display unblocking icon -");
  await waitForTabBlockEvent(tab, false);

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
});
