const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

var SuspendedType = {
  NONE_SUSPENDED             : 0,
  SUSPENDED_PAUSE            : 1,
  SUSPENDED_BLOCK            : 2,
  SUSPENDED_PAUSE_DISPOSABLE : 3
};

function wait_for_event(browser, event) {
  return BrowserTestUtils.waitForEvent(browser, event, false, (event) => {
    is(event.originalTarget, browser, "Event must be dispatched to correct browser.");
    return true;
  });
}

function check_all_audio_suspended(suspendedType) {
  var autoPlay = content.document.getElementById('autoplay');
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!autoPlay || !nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
  is(nonAutoPlay.computedSuspended, suspendedType,
     "The suspeded state of non-autoplay audio is correct.");
}

function check_autoplay_audio_suspended(suspendedType) {
  var autoPlay = content.document.getElementById('autoplay');
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
}

function check_nonautoplay_audio_suspended(suspendedType) {
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(nonAutoPlay.computedSuspended, suspendedType,
     "The suspeded state of non-autoplay audio is correct.");
}

function check_autoplay_audio_pause_state(expectedPauseState) {
  var autoPlay = content.document.getElementById('autoplay');
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  if (autoPlay.paused == expectedPauseState) {
    if (expectedPauseState) {
      ok(true, "Audio is paused correctly.");
    } else {
      ok(true, "Audio is resumed correctly.");
    }
  } else if (expectedPauseState) {
    autoPlay.onpause = function() {
      autoPlay.onpause = null;
      ok(true, "Audio is paused correctly, checking from onpause.");
    }
  } else {
    autoPlay.onplay = function() {
      autoPlay.onplay = null;
      ok(true, "Audio is resumed correctly, checking from onplay.");
    }
  }
}

function play_nonautoplay_audio_should_be_paused() {
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  nonAutoPlay.play();
  return new Promise(resolve => {
    nonAutoPlay.onpause = function() {
      nonAutoPlay.onpause = null;
      is(nonAutoPlay.ended, false, "Audio can't be playback.");
      resolve();
    }
  });
}

function all_audio_onresume() {
  var autoPlay = content.document.getElementById('autoplay');
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!autoPlay || !nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.paused, false, "Autoplay audio is resumed.");
  is(nonAutoPlay.paused, false, "Non-AutoPlay audio is resumed.");
}

function all_audio_onpause() {
  var autoPlay = content.document.getElementById('autoplay');
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!autoPlay || !nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.paused, true, "Autoplay audio is paused.");
  is(nonAutoPlay.paused, true, "Non-AutoPlay audio is paused.");
}

function play_nonautoplay_audio_should_play_until_ended() {
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  nonAutoPlay.play();
  return new Promise(resolve => {
    nonAutoPlay.onended = function() {
      nonAutoPlay.onended = null;
      ok(true, "Audio can be playback until ended.");
      resolve();
    }
  });
}

function no_audio_resumed() {
  var autoPlay = content.document.getElementById('autoplay');
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!autoPlay || !nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.paused && nonAutoPlay.paused, true, "No audio was resumed.");
}

function play_nonautoplay_audio_should_be_blocked(suspendedType) {
  var nonAutoPlay = content.document.getElementById('nonautoplay');
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  nonAutoPlay.play();
  ok(nonAutoPlay.paused, "The blocked audio can't be playback.");
}

function* suspended_pause(url, browser) {
  info("### Start test for suspended-pause ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the default suspended state of all audio should be non-suspened-");
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- pause all audio in the page -");
  browser.pauseMedia(false /* non-disposable */);
  yield ContentTask.spawn(browser, true /* expect for pause */,
                                   check_autoplay_audio_pause_state);
  yield ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE,
                                   check_autoplay_audio_suspended);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_nonautoplay_audio_suspended);

  info("- no audio can be playback during suspended-paused -");
  yield ContentTask.spawn(browser, null,
                                   play_nonautoplay_audio_should_be_paused);
  yield ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE,
                                   check_nonautoplay_audio_suspended);

  info("- both audio should be resumed at the same time -");
  browser.resumeMedia();
  yield ContentTask.spawn(browser, null,
                                   all_audio_onresume);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- both audio should be paused at the same time -");
  browser.pauseMedia(false /* non-disposable */);
  yield ContentTask.spawn(browser, null, all_audio_onpause);
}

function* suspended_pause_disposable(url, browser) {
  info("### Start test for suspended-pause-disposable ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the default suspended state of all audio should be non-suspened -");
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- only pause playing audio in the page -");
  browser.pauseMedia(true /* non-disposable */);
  yield ContentTask.spawn(browser, true /* expect for pause */,
                                   check_autoplay_audio_pause_state);
  yield ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE_DISPOSABLE,
                                   check_autoplay_audio_suspended);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_nonautoplay_audio_suspended);

  info("- new playing audio should be playback correctly -");
  yield ContentTask.spawn(browser, null,
                                   play_nonautoplay_audio_should_play_until_ended);

  info("- should only resume one audio -");
  browser.resumeMedia();
  yield ContentTask.spawn(browser, false /* expect for playing */,
                                   check_autoplay_audio_pause_state);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);
}

function* suspended_stop_disposable(url, browser) {
  info("### Start test for suspended-stop-disposable ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the default suspended state of all audio should be non-suspened -");
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- only stop playing audio in the page -");
  browser.stopMedia();
  yield wait_for_event(browser, "DOMAudioPlaybackStopped");
  yield ContentTask.spawn(browser, true /* expect for pause */,
                                   check_autoplay_audio_pause_state);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- new playing audio should be playback correctly -");
  yield ContentTask.spawn(browser, null,
                                   play_nonautoplay_audio_should_play_until_ended);

  info("- no any audio can be resumed by page -");
  browser.resumeMedia();
  yield ContentTask.spawn(browser, null, no_audio_resumed);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);
}

function* suspended_block(url, browser) {
  info("### Start test for suspended-block ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the default suspended state of all audio should be non-suspened-");
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- block autoplay audio -");
  browser.blockMedia();
  yield ContentTask.spawn(browser, SuspendedType.SUSPENDED_BLOCK,
                                   check_autoplay_audio_suspended);
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_nonautoplay_audio_suspended);

  info("- no audio can be playback during suspended-block -");
  yield ContentTask.spawn(browser, SuspendedType.SUSPENDED_BLOCK,
                                   play_nonautoplay_audio_should_be_blocked);

  info("- both audio should be resumed at the same time -");
  browser.resumeMedia();
  yield ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);
}

add_task(function* setup_test_preference() {
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["media.useAudioChannelService.testing", true]
    ]}, resolve);
  });
});

add_task(function* test_suspended_pause() {
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_pause.bind(this, PAGE));
});

add_task(function* test_suspended_pause_disposable() {
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_pause_disposable.bind(this, PAGE));
});

add_task(function* test_suspended_stop_disposable() {
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_stop_disposable.bind(this, PAGE));
});

add_task(function* test_suspended_block() {
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_block.bind(this, PAGE));
});
