/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We need to request longer timeout because HTTPS-Only Mode sends the
// backround http request with a delay of N milliseconds before the
// actual load gets cancelled.
requestLongerTimeout(5);

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_PATH_HTTPS = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TIMEOUT_PAGE_URI_HTTP =
  TEST_PATH_HTTP + "file_errorpage_timeout_server.sjs";
const TIMEOUT_PAGE_URI_HTTPS =
  TEST_PATH_HTTPS + "file_errorpage_timeout_server.sjs";

add_task(async function avoid_timeout_and_show_https_only_error_page() {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      false, // includeSubFrames = false, no need to includeSubFrames
      TIMEOUT_PAGE_URI_HTTPS, // Wait for upgraded page to timeout
      true // maybeErrorPage = true, because we need the error page to appear
    );
    BrowserTestUtils.startLoadingURIString(browser, TIMEOUT_PAGE_URI_HTTP);
    await loaded;

    await SpecialPowers.spawn(browser, [], async function () {
      const doc = content.document;
      let errorPage = doc.body.innerHTML;
      // It's possible that fluent has not been translated when running in
      // chaos mode, hence let's rather use an element id for verification
      // that the https-only mode error page has loaded.
      ok(
        errorPage.includes("about-httpsonly-button-continue-to-site"),
        "Potential time-out in https-only mode should cause error page to appear!"
      );
      // Verify that the right title is set.
      ok(
        errorPage.includes("about-httpsonly-title-site-not-available"),
        "Potential time-out in https-only mode should cause error page to appear with right title!"
      );
    });
  });
});
