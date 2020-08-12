/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We need to request longer timeout because HTTPS-Only Mode sends the
// backround http request with a delay of N milliseconds before the
// actual load gets cancelled.
requestLongerTimeout(5);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TIMEOUT_PAGE_URI = TEST_PATH + "file_errorpage_timeout_server.sjs";

add_task(async function avoid_timeout_and_show_https_only_error_page() {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      false, // includeSubFrames = false, no need to includeSubFrames
      TIMEOUT_PAGE_URI,
      true // maybeErrorPage = true, because we need the error page to appear
    );
    BrowserTestUtils.loadURI(browser, TIMEOUT_PAGE_URI);
    await loaded;

    await SpecialPowers.spawn(browser, [], async function() {
      const doc = content.document;
      let errorPage = doc.body.innerHTML;
      // It's possible that fluent has not been translated when running in
      // chaos mode, hence let's rather use an element id for verification
      // that the https-only mode error page has loaded.
      ok(
        errorPage.includes("about-httpsonly-button-accept-and-continue"),
        "Potential time-out in https-only mode should cause error page to appear!"
      );
    });
  });
});
