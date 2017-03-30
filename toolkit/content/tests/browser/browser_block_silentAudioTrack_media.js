const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_silentAudioTrack.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3
};

function* click_unblock_icon(tab) {
  let icon = document.getAnonymousElementByAttribute(tab, "anonid", "soundplaying-icon");

  yield hover_icon(icon, document.getElementById("tabbrowser-tab-tooltip"));
  EventUtils.synthesizeMouseAtCenter(icon, {button: 0});
  leave_icon(icon);
}

function check_audio_suspended(suspendedType) {
  var autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
}

add_task(function* setup_test_preference() {
  yield SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(function* unblock_icon_should_disapear_after_resume_tab() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- the suspend state of tab should be suspend-block -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- tab should display unblocking icon -");
  yield waitForTabBlockEvent(tab, true);

  info("- select tab as foreground tab -");
  yield BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- the suspend state of tab should be none-suspend -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should not display unblocking icon -");
  yield waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  yield waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* should_not_show_sound_indicator_after_resume_tab() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- the suspend state of tab should be suspend-block -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- tab should display unblocking icon -");
  yield waitForTabBlockEvent(tab, true);

  info("- click play tab icon -");
  yield click_unblock_icon(tab);

  info("- the suspend state of tab should be none-suspend -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should not display unblocking icon -");
  yield waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  yield waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});
