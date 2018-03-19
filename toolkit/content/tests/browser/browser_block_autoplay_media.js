const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3
};

function check_audio_suspended(suspendedType) {
  var autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
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
  await ContentTask.spawn(tab1.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- open new background tab2 -");
  let tab2 = window.gBrowser.addTab("about:blank");
  tab2.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  info("- should block autoplay for non-visited tab2 -");
  await ContentTask.spawn(tab2.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- select tab1 as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab1);

  info("- media should be unblocked because the tab was visited -");
  await waitForTabPlayingEvent(tab1, true);
  await ContentTask.spawn(tab1.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- open another new foreground tab3 -");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                         "about:blank");
  info("- should still play media from tab1 -");
  await waitForTabPlayingEvent(tab1, true);
  await ContentTask.spawn(tab1.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should still block media from tab2 -");
  await waitForTabPlayingEvent(tab2, false);
  await ContentTask.spawn(tab2.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- remove tabs -");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
