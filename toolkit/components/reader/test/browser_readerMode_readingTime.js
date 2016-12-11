/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

/**
 * Test that the reader mode correctly calculates and displays the
 * estimated reading time for a normal length article
 */
add_task(function* () {
  yield BrowserTestUtils.withNewTab(TEST_PATH + "readerModeArticle.html", function* (browser) {
    let pageShownPromise = BrowserTestUtils.waitForContentEvent(browser, "AboutReaderContentReady");
    let readerButton = document.getElementById("reader-mode-button");
    readerButton.click();
    yield pageShownPromise;
    yield ContentTask.spawn(browser, null, function* () {
      // make sure there is a reading time on the page and that it displays the correct information
      let readingTimeElement = content.document.getElementById("reader-estimated-time");
      ok(readingTimeElement, "Reading time element should be in document");
      is(readingTimeElement.textContent, "9-12 min", "Reading time should be '9-12 min'");
    });
  });
});

/**
 * Test that the reader mode correctly calculates and displays the
 * estimated reading time for a short article
 */
add_task(function* () {
  yield BrowserTestUtils.withNewTab(TEST_PATH + "readerModeArticleShort.html", function* (browser) {
    let pageShownPromise = BrowserTestUtils.waitForContentEvent(browser, "AboutReaderContentReady");
    let readerButton = document.getElementById("reader-mode-button");
    readerButton.click();
    yield pageShownPromise;
    yield ContentTask.spawn(browser, null, function* () {
      // make sure there is a reading time on the page and that it displays the correct information
      let readingTimeElement = content.document.getElementById("reader-estimated-time");
      ok(readingTimeElement, "Reading time element should be in document");
      is(readingTimeElement.textContent, "1 min", "Reading time should be '1 min'");
    });
  });
});
