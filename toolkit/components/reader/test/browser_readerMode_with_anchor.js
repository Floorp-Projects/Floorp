/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(async function() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "readerModeArticle.html#foo", async function(browser) {
    let pageShownPromise = BrowserTestUtils.waitForContentEvent(browser, "AboutReaderContentReady");
    let readerButton = document.getElementById("reader-mode-button");
    readerButton.click();
    await pageShownPromise;
    await ContentTask.spawn(browser, null, async function() {
      let foo = content.document.getElementById("foo");
      ok(foo, "foo element should be in document");
      let {scrollTop} = content.document.documentElement;
      let {offsetTop} = foo;
      Assert.lessOrEqual(Math.abs(scrollTop - offsetTop), 1,
        `scrollTop (${scrollTop}) should be within 1 CSS pixel of offsetTop (${offsetTop})`);
    });
  });
});

add_task(async function() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "readerModeArticle.html", async function(browser) {
    let pageShownPromise = BrowserTestUtils.waitForContentEvent(browser, "AboutReaderContentReady");
    let readerButton = document.getElementById("reader-mode-button");
    readerButton.click();
    await pageShownPromise;
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    is(gBrowser.contentDocumentAsCPOW.documentElement.scrollTop, 0, "scrollTop should be 0");
    await BrowserTestUtils.synthesizeMouseAtCenter("#foo-anchor", {}, browser);
    await ContentTask.spawn(browser, null, async function() {
      let foo = content.document.getElementById("foo");
      ok(foo, "foo element should be in document");
      let {scrollTop} = content.document.documentElement;
      let {offsetTop} = foo;
      Assert.lessOrEqual(Math.abs(scrollTop - offsetTop), 1,
        `scrollTop (${scrollTop}) should be within 1 CSS pixel of offsetTop (${offsetTop})`);
    });
  });
});
