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
    "The pause state of audio is corret.")
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
    }
  });
}

add_task(function* setup_test_preference() {
  yield SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});


/**
 * This test is used for testing the visible tab which was not resumed yet.
 * If the tab doesn't have any media component, it won't be resumed even it
 * has already gone to foreground until we start audio.
 */
add_task(function* media_should_be_able_to_play_in_visible_tab() {
  info("- open new background tab, and check tab's media pause state -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield ContentTask.spawn(tab.linkedBrowser, true,
                          check_audio_pause_state);

  info("- select tab as foreground tab, and tab's media should still be paused -");
  yield BrowserTestUtils.switchTab(window.gBrowser, tab);
  yield ContentTask.spawn(tab.linkedBrowser, true,
                          check_audio_pause_state);

  info("- start audio in tab -");
  yield ContentTask.spawn(tab.linkedBrowser, null,
                          play_audio);

  info("- audio should be playing -");
  yield ContentTask.spawn(tab.linkedBrowser, false,
                          check_audio_pause_state);
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});
