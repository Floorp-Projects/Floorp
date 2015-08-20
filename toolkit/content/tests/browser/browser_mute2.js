const PAGE = "data:text/html,page";

function* test_on_browser(browser) {
  ok(!browser.audioMuted, "Audio should not be muted by default");
  browser.mute();
  ok(browser.audioMuted, "Audio should be muted now");

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, test_on_browser2);

  browser.unmute();
  ok(!browser.audioMuted, "Audio should be unmuted now");
}

function* test_on_browser2(browser) {
  ok(!browser.audioMuted, "Audio should not be muted by default");
}

add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, test_on_browser);
});
