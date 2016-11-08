/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(function* () {
  yield BrowserTestUtils.withNewTab(TEST_PATH + "readerModeArticle.html#foo", function* (browser) {
    let pageShownPromise = BrowserTestUtils.waitForContentEvent(browser, "AboutReaderContentReady");
    let readerButton = document.getElementById("reader-mode-button");
    readerButton.click();
    yield pageShownPromise;
    yield ContentTask.spawn(browser, null, function* () {
      // Check if offset != 0
      ok(content.document.getElementById("foo") !== null, "foo element should be in document");
      ok(content.pageYOffset != 0, "pageYOffset should be > 0");
    });
  });
});
