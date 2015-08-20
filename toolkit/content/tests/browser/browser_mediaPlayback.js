const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlayback.html";
const FRAME = "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlaybackFrame.html";

function wait_for_event(browser, event) {
  return BrowserTestUtils.waitForEvent(browser, event, false, (event) => {
    is(event.originalTarget, browser, "Event must be dispatched to correct browser.");
    ok(!event.cancelable, "The event should not be cancelable");
    return true;
  });
}

function* test_on_browser(url, browser) {
  browser.loadURI(url);
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");
  yield wait_for_event(browser, "DOMAudioPlaybackStopped");
}

add_task(function* test_page() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, test_on_browser.bind(undefined, PAGE));
});

add_task(function* test_frame() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, test_on_browser.bind(undefined, FRAME));
});
