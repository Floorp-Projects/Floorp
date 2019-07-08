const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_plugIn.html";

add_task(async function setup_test_preference() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["media.block-autoplay-until-in-foreground", true],
    ],
  });
});

add_task(async function block_plug_in() {
  info("- open new background tab -");
  let tab = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  BrowserTestUtils.loadURI(tab.linkedBrowser, PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- tab should be blocked -");
  await waitForTabBlockEvent(tab, true);

  info("- switch tab -");
  await BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- tab should be resumed -");
  await waitForTabBlockEvent(tab, false);

  info("- tab should be audible -");
  await waitForTabPlayingEvent(tab, true);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
