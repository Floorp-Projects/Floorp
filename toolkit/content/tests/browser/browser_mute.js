const PAGE = "data:text/html,page";

function* test_on_browser(browser) {
  ok(!browser.audioMuted, "Audio should not be muted by default");
  browser.mute();
  ok(browser.audioMuted, "Audio should be muted now");
  browser.unmute();
  ok(!browser.audioMuted, "Audio should be unmuted now");
}

add_task(function*() {
  yield new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": [["media.useAudioChannelService", true]]},
                              resolve);
  });
});

add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, test_on_browser);
});
