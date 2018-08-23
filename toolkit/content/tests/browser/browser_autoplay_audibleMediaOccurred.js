/**
 * This test is used to test whether the topic 'AudibleAutoplayMediaOccurred'
 * is sent correctly when the autoplay audible media tries to start.
 */
"use strict";

const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlayback.html";

add_task(async function testAudibleAutoplayMedia() {
  info("- open new tab  -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  let browser = tab.linkedBrowser;

  // start observing the topic before loading the page to ensure we can get it.
  let audibleAutoplayOccurred = TestUtils.topicObserved("AudibleAutoplayMediaOccurred");
  browser.loadURI(PAGE);

  await BrowserTestUtils.browserLoaded(browser);
  await audibleAutoplayOccurred;
  ok(true, "Got the topic 'AudibleAutoplayMediaOccurred'.");

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
