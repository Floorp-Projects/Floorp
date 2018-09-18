const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_silentAudioTrack.html";

var SuspendedType = {
  NONE_SUSPENDED: 0,
  SUSPENDED_PAUSE: 1,
  SUSPENDED_BLOCK: 2,
  SUSPENDED_PAUSE_DISPOSABLE: 3,
};

async function click_unblock_icon(tab) {
  let icon = document.getAnonymousElementByAttribute(tab, "anonid", "soundplaying-icon");

  await hover_icon(icon, document.getElementById("tabbrowser-tab-tooltip"));
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

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true],
  ]});
});

add_task(async function unblock_icon_should_disapear_after_resume_tab() {
  info("- open new background tab -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- the suspend state of tab should be suspend-block -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- tab should display unblocking icon -");
  await waitForTabBlockEvent(tab, true);

  info("- select tab as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- the suspend state of tab should be none-suspend -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should not display unblocking icon -");
  await waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  await waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function should_not_show_sound_indicator_after_resume_tab() {
  info("- open new background tab -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- the suspend state of tab should be suspend-block -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- tab should display unblocking icon -");
  await waitForTabBlockEvent(tab, true);

  info("- click play tab icon -");
  await click_unblock_icon(tab);

  info("- the suspend state of tab should be none-suspend -");
  await ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should not display unblocking icon -");
  await waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  await waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
