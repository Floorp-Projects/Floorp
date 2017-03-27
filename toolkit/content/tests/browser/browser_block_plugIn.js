const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_plugIn.html";

add_task(function* setup_test_preference() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  yield SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(function* block_plug_in() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- tab should be blocked -");
  yield waitForTabBlockEvent(tab, true);

  info("- switch tab -");
  yield BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- tab should be resumed -");
  yield waitForTabBlockEvent(tab, false);

  info("- tab should be audible -");
  yield waitForTabPlayingEvent(tab, true);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});
