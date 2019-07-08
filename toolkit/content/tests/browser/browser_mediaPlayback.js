const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlayback.html";
const FRAME =
  "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlaybackFrame.html";

function wait_for_event(browser, event) {
  return BrowserTestUtils.waitForEvent(browser, event, false, event => {
    is(
      event.originalTarget,
      browser,
      "Event must be dispatched to correct browser."
    );
    ok(!event.cancelable, "The event should not be cancelable");
    return true;
  });
}

async function test_on_browser(url, browser) {
  BrowserTestUtils.loadURI(browser, url);
  await wait_for_event(browser, "DOMAudioPlaybackStarted");
  await wait_for_event(browser, "DOMAudioPlaybackStopped");
}

add_task(async function test_page() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    test_on_browser.bind(undefined, PAGE)
  );
});

add_task(async function test_frame() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    test_on_browser.bind(undefined, FRAME)
  );
});
