const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3
};

function check_audio_suspended(browser, suspendedType) {
  return ContentTask.spawn(browser, suspendedType, suspendedType => {
    var autoPlay = content.document.getElementById("autoplay");
    if (!autoPlay) {
      ok(false, "Can't get the audio element!");
    }
    is(autoPlay.computedSuspended, suspendedType,
       "The suspeded state of autoplay audio is correct.");
  });
}

function check_audio_paused(browser, shouldBePaused) {
  return ContentTask.spawn(browser, shouldBePaused, shouldBePaused => {
    var autoPlay = content.document.getElementById("autoplay");
    if (!autoPlay) {
      ok(false, "Can't get the audio element!");
    }
    is(autoPlay.paused, shouldBePaused,
      "autoplay audio should " + (!shouldBePaused ? "not " : "") + "be paused.");
  });
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(async function block_autoplay_media() {
  info("- open new background tab1 -");
  let tab1 = window.gBrowser.addTab("about:blank");
  tab1.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);

  info("- should block autoplay media for non-visited tab1 -");
  await check_audio_suspended(tab1.linkedBrowser, SuspendedType.SUSPENDED_BLOCK);

  info("- open new background tab2 -");
  let tab2 = window.gBrowser.addTab("about:blank");
  tab2.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  info("- should block autoplay for non-visited tab2 -");
  await check_audio_suspended(tab2.linkedBrowser, SuspendedType.SUSPENDED_BLOCK);

  info("- select tab1 as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab1);

  info("- media should be unblocked because the tab was visited -");
  await waitForTabPlayingEvent(tab1, true);
  await check_audio_suspended(tab1.linkedBrowser, SuspendedType.NONE_SUSPENDED);

  info("- open another new foreground tab3 -");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                         "about:blank");
  info("- should still play media from tab1 -");
  await waitForTabPlayingEvent(tab1, true);
  await check_audio_suspended(tab1.linkedBrowser, SuspendedType.NONE_SUSPENDED);

  info("- should still block media from tab2 -");
  await waitForTabPlayingEvent(tab2, false);
  await check_audio_suspended(tab2.linkedBrowser, SuspendedType.SUSPENDED_BLOCK);


  // Test 4: Disable autoplay, enable asking for permission, and verify
  // that when a tab is opened in the background and has had its playback
  // start delayed, resuming via the audio tab indicator overrides the
  // autoplay blocking logic.
  //
  // Clicking "play" on the audio tab indicator should always start playback
  // in that tab, even if it's in an autoplay-blocked origin.
  //
  // Also test that that this block-autoplay logic override doesn't survive
  // a new document being loaded into the tab; the new document should have
  // to satisfy the autoplay requirements on its own.
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.enabled", false],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.ask-permission", true],
  ]});

  info("- open new background tab4 -");
  let tab4 = window.gBrowser.addTab("about:blank");
  tab4.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab4.linkedBrowser);
  info("- should block autoplay for non-visited tab4 -");
  await check_audio_suspended(tab4.linkedBrowser, SuspendedType.SUSPENDED_BLOCK);
  await check_audio_paused(tab4.linkedBrowser, true);
  tab4.linkedBrowser.resumeMedia();
  info("- should not block media from tab4 -");
  await waitForTabPlayingEvent(tab4, true);
  await check_audio_paused(tab4.linkedBrowser, false);

  info("- check that loading a new URI in page clears gesture activation status -");
  tab4.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab4.linkedBrowser);
  info("- should block autoplay again as gesture activation status cleared -");
  await check_audio_paused(tab4.linkedBrowser, true);

  info("- remove tabs -");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
});
