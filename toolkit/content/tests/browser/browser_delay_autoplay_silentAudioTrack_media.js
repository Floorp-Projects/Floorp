const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_silentAudioTrack.html";

async function click_unblock_icon(tab) {
  let icon = tab.overlayIcon;

  await hover_icon(icon, document.getElementById("tabbrowser-tab-tooltip"));
  EventUtils.synthesizeMouseAtCenter(icon, { button: 0 });
  leave_icon(icon);
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

add_task(async function unblock_icon_should_disapear_after_resume_tab() {
  info("- open new background tab -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- tab should display unblocking icon -");
  await waitForTabBlockEvent(tab, true);

  info("- select tab as foreground tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

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
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- tab should display unblocking icon -");
  await waitForTabBlockEvent(tab, true);

  info("- click play tab icon -");
  await click_unblock_icon(tab);

  info("- should not display unblocking icon -");
  await waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  await waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
