const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_webAudio.html";

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(async function block_web_audio() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
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
  await BrowserTestUtils.removeTab(tab);
});
