const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";
const PAGE_NO_AUTOPLAY =
  "https://example.com/browser/toolkit/content/tests/browser/file_nonAutoplayAudio.html";

function check_audio_paused(browser, shouldBePaused) {
  return SpecialPowers.spawn(browser, [shouldBePaused], shouldBePaused => {
    var autoPlay = content.document.getElementById("autoplay");
    if (!autoPlay) {
      ok(false, "Can't get the audio element!");
    }
    is(
      autoPlay.paused,
      shouldBePaused,
      "autoplay audio should " + (!shouldBePaused ? "not " : "") + "be paused."
    );
  });
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

function set_media_autoplay() {
  let audio = content.document.getElementById("testAudio");
  if (!audio) {
    ok(false, "Can't get the audio element!");
    return;
  }
  audio.autoplay = true;
}

add_task(async function delay_media_with_autoplay_keyword() {
  info("- open new background tab -");
  const tab = BrowserTestUtils.addTab(window.gBrowser, PAGE_NO_AUTOPLAY);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- set media's autoplay property -");
  await SpecialPowers.spawn(tab.linkedBrowser, [], set_media_autoplay);

  info("- should delay autoplay media -");
  await waitForTabBlockEvent(tab, true);

  info("- switch tab to foreground -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- media should be resumed because tab has been visited -");
  await waitForTabPlayingEvent(tab, true);
  await waitForTabBlockEvent(tab, false);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function delay_media_with_play_invocation() {
  info("- open new background tab1 -");
  let tab1 = BrowserTestUtils.addTab(window.gBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);

  info("- should delay autoplay media for non-visited tab1 -");
  await waitForTabBlockEvent(tab1, true);

  info("- open new background tab2 -");
  let tab2 = BrowserTestUtils.addTab(window.gBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  info("- should delay autoplay for non-visited tab2 -");
  await waitForTabBlockEvent(tab2, true);

  info("- switch to tab1 -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab1);

  info("- media in tab1 should be unblocked because the tab was visited -");
  await waitForTabPlayingEvent(tab1, true);
  await waitForTabBlockEvent(tab1, false);

  info("- open another new foreground tab3 -");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  info("- should still play media from tab1 -");
  await waitForTabPlayingEvent(tab1, true);

  info("- should still block media from tab2 -");
  await waitForTabPlayingEvent(tab2, false);
  await waitForTabBlockEvent(tab2, true);

  info("- remove tabs -");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

add_task(async function resume_delayed_media_when_enable_blocking_autoplay() {
  // Disable autoplay and verify that when a tab is opened in the
  // background and has had its playback start delayed, resuming via the audio
  // tab indicator overrides the autoplay blocking logic.
  //
  // Clicking "play" on the audio tab indicator should always start playback
  // in that tab, even if it's in an autoplay-blocked origin.
  //
  // Also test that that this block-autoplay logic override doesn't survive
  // a new document being loaded into the tab; the new document should have
  // to satisfy the autoplay requirements on its own.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.blocking_policy", 0],
    ],
  });

  info("- open new background tab -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- should block autoplay for non-visited tab -");
  await waitForTabBlockEvent(tab, true);
  await check_audio_paused(tab.linkedBrowser, true);
  tab.linkedBrowser.resumeMedia();

  info("- should not block media from tab -");
  await waitForTabPlayingEvent(tab, true);
  await check_audio_paused(tab.linkedBrowser, false);

  info(
    "- check that loading a new URI in page clears gesture activation status -"
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- should block autoplay again as gesture activation status cleared -");
  await check_audio_paused(tab.linkedBrowser, true);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);

  // Clear the block-autoplay pref.
  await SpecialPowers.popPrefEnv();
});
