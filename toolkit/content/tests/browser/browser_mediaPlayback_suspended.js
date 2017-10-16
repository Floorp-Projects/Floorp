const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlayback2.html";

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

function check_audio_suspended(suspendedType) {
  var list = content.document.getElementsByTagName("audio");
  if (list.length != 1) {
    ok(false, "There should be only one audio element in page!");
  }

  var audio = list[0];
  is(audio.computedSuspended, suspendedType,
     "The suspended state of MediaElement is correct.");
}

function check_audio_pause_state(expectedPauseState) {
  var list = content.document.getElementsByTagName("audio");
  if (list.length != 1) {
    ok(false, "There should be only one audio element in page!");
  }

  var audio = list[0];
  if (expectedPauseState) {
    is(audio.paused, true, "Audio is paused correctly.");
  } else {
    is(audio.paused, false, "Audio is resumed correctly.");
  }
}

async function suspended_pause(url, browser) {
  info("### Start test for suspended-pause ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  await wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the suspended state of audio should be non-suspened -");
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio_suspended);

  info("- pause playing audio -");
  browser.pauseMedia(false /* non-disposable */);
  await ContentTask.spawn(browser, true /* expect for pause */,
                                   check_audio_pause_state);
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE,
                                   check_audio_suspended);

  info("- resume paused audio -");
  browser.resumeMedia();
  await ContentTask.spawn(browser, false /* expect for playing */,
                                   check_audio_pause_state);
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio_suspended);
}

async function suspended_pause_disposable(url, browser) {
  info("### Start test for suspended-pause-disposable ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  await wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the suspended state of audio should be non-suspened -");
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio_suspended);

  info("- pause playing audio -");
  browser.pauseMedia(true /* disposable */);
  await ContentTask.spawn(browser, true /* expect for pause */,
                                   check_audio_pause_state);
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_PAUSE_DISPOSABLE,
                                   check_audio_suspended);

  info("- resume paused audio -");
  browser.resumeMedia();
  await ContentTask.spawn(browser, false /* expect for playing */,
                                   check_audio_pause_state);
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio_suspended);
}

async function suspended_stop_disposable(url, browser) {
  info("### Start test for suspended-stop-disposable ###");
  browser.loadURI(url);

  info("- page should have playing audio -");
  await wait_for_event(browser, "DOMAudioPlaybackStarted");

  info("- the suspended state of audio should be non-suspened -");
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio_suspended);

  info("- stop playing audio -");
  browser.stopMedia();
  await wait_for_event(browser, "DOMAudioPlaybackStopped");
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_audio_suspended);
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true]
  ]});
});

add_task(async function test_suspended_pause() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_pause.bind(this, PAGE));
});

add_task(async function test_suspended_pause_disposable() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_pause_disposable.bind(this, PAGE));
});

add_task(async function test_suspended_stop_disposable() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "about:blank"
    }, suspended_stop_disposable.bind(this, PAGE));
});
