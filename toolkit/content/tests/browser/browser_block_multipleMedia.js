const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3
};

function check_all_audio_suspended(suspendedType) {
  let autoPlay = content.document.getElementById("autoplay");
  let nonAutoPlay = content.document.getElementById("nonautoplay");
  if (!autoPlay || !nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
  is(nonAutoPlay.computedSuspended, suspendedType,
     "The suspeded state of non-autoplay audio is correct.");
}

function check_autoplay_audio_suspended(suspendedType) {
  let autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
}

function check_autoplay_audio_onplay() {
  let autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  return new Promise((resolve, reject) => {
    autoPlay.onplay = () => {
      ok(false, "Should not receive play event!");
      this.onplay = null;
      reject();
    };

    autoPlay.pause();
    autoPlay.play();

    setTimeout(() => {
      ok(true, "Doesn't receive play event when media was blocked.");
      autoPlay.onplay = null;
      resolve();
    }, 1000)
  });
}

function check_nonautoplay_audio_suspended(suspendedType) {
  let nonAutoPlay = content.document.getElementById("nonautoplay");
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(nonAutoPlay.computedSuspended, suspendedType,
     "The suspeded state of non-autoplay audio is correct.");
}

function play_nonautoplay_audio_should_be_blocked() {
  let nonAutoPlay = content.document.getElementById("nonautoplay");
  if (!nonAutoPlay) {
    ok(false, "Can't get the audio element!");
  }

  nonAutoPlay.play();
  ok(nonAutoPlay.paused, "The blocked audio can't be playback.");
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(async function block_multiple_media() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  browser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  info("- tab should be blocked -");
  await waitForTabBlockEvent(tab, true);

  info("- autoplay media should be blocked -");
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_BLOCK,
                                   check_autoplay_audio_suspended);
  await ContentTask.spawn(browser, null,
                                   check_autoplay_audio_onplay);

  info("- non-autoplay media won't be blocked, because it doesn't start playing -");
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_nonautoplay_audio_suspended);

  info("- non-autoplay can't start playback when the tab is blocked -");
  await ContentTask.spawn(browser, null,
                                   play_nonautoplay_audio_should_be_blocked);
  await ContentTask.spawn(browser, SuspendedType.SUSPENDED_BLOCK,
                                   check_nonautoplay_audio_suspended);

  info("- select tab as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- tab should be resumed -");
  await waitForTabPlayingEvent(tab, true);
  await ContentTask.spawn(browser, SuspendedType.NONE_SUSPENDED,
                                   check_all_audio_suspended);

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
});
