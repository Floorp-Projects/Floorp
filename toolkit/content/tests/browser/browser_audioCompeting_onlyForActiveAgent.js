const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_multiplePlayingAudio.html";

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

function check_all_audio_suspended(suspendedType) {
  var audio1 = content.document.getElementById("audio1");
  var audio2 = content.document.getElementById("audio2");
  if (!audio1 || !audio2) {
    ok(false, "Can't get the audio element!");
  }

  is(audio1.computedSuspended, suspendedType,
     "The suspeded state of audio1 is correct.");
  is(audio2.computedSuspended, suspendedType,
     "The suspeded state of audio2 is correct.");
}

function check_audio1_suspended(suspendedType) {
  var audio1 = content.document.getElementById("audio1");
  if (!audio1) {
    ok(false, "Can't get the audio element!");
  }

  is(audio1.computedSuspended, suspendedType,
     "The suspeded state of audio1 is correct.");
}

function check_audio2_suspended(suspendedType) {
  var audio2 = content.document.getElementById("audio2");
  if (!audio2) {
    ok(false, "Can't get the audio element!");
  }

  is(audio2.computedSuspended, suspendedType,
     "The suspeded state of audio2 is correct.");
}

function check_all_audio_pause_state(expectedPauseState) {
  var audio1 = content.document.getElementById("audio1");
  var audio2 = content.document.getElementById("audio2");
  if (!audio1 | !audio2) {
    ok(false, "Can't get the audio element!");
  }

  is(audio1.paused, expectedPauseState,
    "The pause state of audio1 is correct.");
  is(audio2.paused, expectedPauseState,
    "The pause state of audio2 is correct.");
}

function check_audio1_pause_state(expectedPauseState) {
  var audio1 = content.document.getElementById("audio1");
  if (!audio1) {
    ok(false, "Can't get the audio element!");
  }

  is(audio1.paused, expectedPauseState,
    "The pause state of audio1 is correct.");
}

function check_audio2_pause_state(expectedPauseState) {
  var audio2 = content.document.getElementById("audio2");
  if (!audio2) {
    ok(false, "Can't get the audio element!");
  }

  is(audio2.paused, expectedPauseState,
    "The pause state of audio2 is correct.");
}

function play_audio1_from_page() {
  var audio1 = content.document.getElementById("audio1");
  if (!audio1) {
    ok(false, "Can't get the audio element!");
  }

  is(audio1.paused, true, "Audio1 is paused.");
  audio1.play();
  return new Promise(resolve => {
    audio1.onplay = function() {
      audio1.onplay = null;
      ok(true, "Audio1 started playing.");
      resolve();
    };
  });
}

function stop_audio1_from_page() {
  var audio1 = content.document.getElementById("audio1");
  if (!audio1) {
    ok(false, "Can't get the audio element!");
  }

  is(audio1.paused, false, "Audio1 is playing.");
  audio1.pause();
  return new Promise(resolve => {
    audio1.onpause = function() {
      audio1.onpause = null;
      ok(true, "Audio1 stopped playing.");
      resolve();
    };
  });
}

async function audio_competing_for_active_agent(url, browser) {
  browser.loadURI(url);

  info("- page should have playing audio -");
  await wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the default suspended state of all audio should be non-suspened -");
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- only pause playing audio in the page -");
  browser.pauseMedia(true /* disposable */);

  info("- page shouldn't have any playing audio -");
  await wait_for_event(browser, "DOMAudioPlaybackStopped");
  await ContentTask.spawn(browser, true /* expect for pause */,
                                   check_all_audio_pause_state);
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE_DISPOSABLE,
                                   check_all_audio_suspended);

  info("- resume audio1 from page -");
  await ContentTask.spawn(browser, null,
                                   play_audio1_from_page);
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio1_suspended);

  info("- audio2 should still be suspended -");
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE_DISPOSABLE,
                                   check_audio2_suspended);
  await ContentTask.spawn(browser, true /* expect for pause */,
                                   check_audio2_pause_state);

  info("- stop audio1 from page -");
  await ContentTask.spawn(browser, null,
                                   stop_audio1_from_page);
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio1_suspended);

  info("- audio2 should still be suspended -");
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE_DISPOSABLE,
                                   check_audio2_suspended);
  await ContentTask.spawn(browser, true /* expect for pause */,
                                   check_audio2_pause_state);

}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["dom.audiochannel.audioCompeting", true],
    ["dom.audiochannel.audioCompeting.allAgents", true]
  ]});
});

add_task(async function test_suspended_pause_disposable() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, audio_competing_for_active_agent.bind(this, PAGE));
});
