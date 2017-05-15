/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that the reader mode button appears and works properly on
 * reader-able content.
 */
const TEST_PREFS = [
  ["reader.parse-on-load.enabled", true],
];

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

var readerButton = document.getElementById("reader-mode-button");

add_task(async function test_reader_button() {
  registerCleanupFunction(function() {
    // Reset test prefs.
    TEST_PREFS.forEach(([name, value]) => {
      Services.prefs.clearUserPref(name);
    });
    while (gBrowser.tabs.length > 1) {
      gBrowser.removeCurrentTab();
    }
  });

  // Set required test prefs.
  TEST_PREFS.forEach(([name, value]) => {
    Services.prefs.setBoolPref(name, value);
  });

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  is_element_hidden(readerButton, "Reader mode button is not present on a new tab");
  ok(!UITour.isInfoOnTarget(window, "readerMode-urlBar"),
     "Info panel shouldn't appear without the reader mode button");

  // Point tab to a test page that is reader-able.
  let url = TEST_PATH + "readerModeArticle.html";
  await promiseTabLoadEvent(tab, url);
  await promiseWaitForCondition(() => !readerButton.hidden);

  is_element_visible(readerButton, "Reader mode button is present on a reader-able page");

  // Switch page into reader mode.
  let promiseTabLoad = promiseTabLoadEvent(tab);
  readerButton.click();
  await promiseTabLoad;

  let readerUrl = gBrowser.selectedBrowser.currentURI.spec;
  ok(readerUrl.startsWith("about:reader"), "about:reader loaded after clicking reader mode button");
  is_element_visible(readerButton, "Reader mode button is present on about:reader");

  is(gURLBar.value, readerUrl, "gURLBar value is about:reader URL");
  is(gURLBar.textValue, url.substring("http://".length), "gURLBar is displaying original article URL");

  // Check selected value for URL bar
  await new Promise((resolve, reject) => {
    waitForClipboard(url, function() {
      gURLBar.focus();
      gURLBar.select();
      goDoCommand("cmd_copy");
    }, resolve, reject);
  });

  info("Got correct URL when copying");

  // Switch page back out of reader mode.
  let promisePageShow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  readerButton.click();
  await promisePageShow;
  is(gBrowser.selectedBrowser.currentURI.spec, url,
    "Back to the original page after clicking active reader mode button");
  ok(gBrowser.selectedBrowser.canGoForward,
    "Moved one step back in the session history.");

  let nonReadableUrl = TEST_PATH + "readerModeNonArticle.html";

  // Load a new tab that is NOT reader-able.
  let newTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await promiseTabLoadEvent(newTab, nonReadableUrl);
  await promiseWaitForCondition(() => readerButton.hidden);
  is_element_hidden(readerButton, "Reader mode button is not present on a non-reader-able page");

  // Switch back to the original tab to make sure reader mode button is still visible.
  gBrowser.removeCurrentTab();
  await promiseWaitForCondition(() => !readerButton.hidden);
  is_element_visible(readerButton, "Reader mode button is present on a reader-able page");

  // Load a new tab in reader mode that is NOT reader-able in the reader mode.
  newTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let promiseAboutReaderError = BrowserTestUtils.waitForContentEvent(newTab.linkedBrowser, "AboutReaderContentError");
  await promiseTabLoadEvent(newTab, "about:reader?url=" + nonReadableUrl);
  await promiseAboutReaderError;
  await promiseWaitForCondition(() => !readerButton.hidden);
  is_element_visible(readerButton, "Reader mode button is present on about:reader even in error state");

  // Switch page back out of reader mode.
  promisePageShow = BrowserTestUtils.waitForContentEvent(newTab.linkedBrowser, "pageshow");
  readerButton.click();
  await promisePageShow;
  is(gBrowser.selectedBrowser.currentURI.spec, nonReadableUrl,
    "Back to the original non-reader-able page after clicking active reader mode button");
  await promiseWaitForCondition(() => readerButton.hidden);
  is_element_hidden(readerButton, "Reader mode button is not present on a non-reader-able page");
});

add_task(async function test_getOriginalUrl() {
  let { ReaderMode } = Cu.import("resource://gre/modules/ReaderMode.jsm", {});
  let url = "http://foo.com/article.html";

  is(ReaderMode.getOriginalUrl("about:reader?url=" + encodeURIComponent(url)), url, "Found original URL from encoded URL");
  is(ReaderMode.getOriginalUrl("about:reader?foobar"), null, "Did not find original URL from malformed reader URL");
  is(ReaderMode.getOriginalUrl(url), null, "Did not find original URL from non-reader URL");

  let badUrl = "http://foo.com/?;$%^^";
  is(ReaderMode.getOriginalUrl("about:reader?url=" + encodeURIComponent(badUrl)), badUrl, "Found original URL from encoded malformed URL");
  is(ReaderMode.getOriginalUrl("about:reader?url=" + badUrl), badUrl, "Found original URL from non-encoded malformed URL");
});

add_task(async function test_reader_view_element_attribute_transform() {
  registerCleanupFunction(function() {
    while (gBrowser.tabs.length > 1) {
      gBrowser.removeCurrentTab();
    }
  });

  function observeAttribute(element, attribute, triggerFn, checkFn) {
    return new Promise(resolve => {
      let observer = new MutationObserver((mutations) => {
        mutations.forEach( mu => {
          if (element.getAttribute(attribute) !== mu.oldValue) {
            checkFn();
            resolve();
            observer.disconnect();
          }
        });
      });

      observer.observe(element, {
        attributes: true,
        attributeOldValue: true,
        attributeFilter: [attribute]
      });

      triggerFn();
    });
  }

  let command = document.getElementById("View:ReaderView");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  is(command.hidden, true, "Command element should have the hidden attribute");

  info("Navigate a reader-able page");
  let waitForPageshow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  await observeAttribute(command, "hidden",
    () => {
      let url = TEST_PATH + "readerModeArticle.html";
      tab.linkedBrowser.loadURI(url);
    },
    () => {
      is(command.hidden, false, "Command's hidden attribute should be false on a reader-able page");
    }
  );
  await waitForPageshow;

  info("Navigate a non-reader-able page");
  waitForPageshow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  await observeAttribute(command, "hidden",
    () => {
      let url = TEST_PATH + "readerModeArticleHiddenNodes.html";
      tab.linkedBrowser.loadURI(url);
    },
    () => {
      is(command.hidden, true, "Command's hidden attribute should be true on a non-reader-able page");
    }
  );
  await waitForPageshow;

  info("Navigate a reader-able page");
  waitForPageshow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  await observeAttribute(command, "hidden",
    () => {
      let url = TEST_PATH + "readerModeArticle.html";
      tab.linkedBrowser.loadURI(url);
    },
    () => {
      is(command.hidden, false, "Command's hidden attribute should be false on a reader-able page");
    }
  );
  await waitForPageshow;

  info("Enter Reader Mode");
  waitForPageshow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  await observeAttribute(readerButton, "readeractive",
    () => {
      readerButton.click();
    },
    () => {
      is(readerButton.getAttribute("readeractive"), "true", "readerButton's readeractive attribute should be true when entering reader mode");
    }
  );
  await waitForPageshow;

  info("Exit Reader Mode");
  waitForPageshow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  await observeAttribute(readerButton, "readeractive",
    () => {
      readerButton.click();
    },
    () => {
      is(readerButton.getAttribute("readeractive"), "", "readerButton's readeractive attribute should be empty when reader mode is exited");
    }
  );
  await waitForPageshow;

  info("Navigate a non-reader-able page");
  waitForPageshow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  await observeAttribute(command, "hidden",
    () => {
      let url = TEST_PATH + "readerModeArticleHiddenNodes.html";
      tab.linkedBrowser.loadURI(url);
    },
    () => {
      is(command.hidden, true, "Command's hidden attribute should be true on a non-reader-able page");
    }
  );
  await waitForPageshow;
});
