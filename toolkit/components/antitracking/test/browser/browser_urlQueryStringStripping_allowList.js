/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_THIRD_PARTY_DOMAIN = TEST_DOMAIN_2;

const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_THIRD_PARTY_URI =
  TEST_THIRD_PARTY_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_REDIRECT_URI = TEST_DOMAIN + TEST_PATH + "redirect.sjs";

const TEST_QUERY_STRING = "paramToStrip=1";

function observeChannel(uri, expected) {
  return TestUtils.topicObserved("http-on-before-connect", (subject, data) => {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    let channelURI = channel.URI;

    if (channelURI.spec.startsWith(uri)) {
      is(
        channelURI.query,
        expected,
        "The loading channel has the expected query string."
      );
      return true;
    }

    return false;
  });
}

async function verifyQueryString(browser, expected) {
  await SpecialPowers.spawn(browser, [expected], expected => {
    // Strip the first question mark.
    let search = content.location.search.slice(1);

    is(search, expected, "The query string is correct.");
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "paramToStrip"],
      ["privacy.query_stripping.redirect", true],
      ["privacy.query_stripping.enabled", true],
    ],
  });

  let listService = Cc[
    "@mozilla.org/query-stripping-list-service;1"
  ].getService(Ci.nsIURLQueryStrippingListService);
  await listService.testWaitForInit();
});

add_task(async function doTestsForTabOpen() {
  let testURI = TEST_URI + "?" + TEST_QUERY_STRING;

  // Observe the channel and check if the query string is stripped.
  let networkPromise = observeChannel(TEST_URI, "");

  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testURI);

  // Verify if the query string is stripped.
  await verifyQueryString(tab.linkedBrowser, "");
  await networkPromise;

  // Toggle ETP off and verify if the query string is restored.
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    testURI
  );
  // Observe the channel and check if the query string is not stripped.
  networkPromise = observeChannel(TEST_URI, TEST_QUERY_STRING);

  gProtectionsHandler.disableForCurrentPage();
  await browserLoadedPromise;
  await networkPromise;

  await verifyQueryString(tab.linkedBrowser, TEST_QUERY_STRING);

  BrowserTestUtils.removeTab(tab);

  // Open the tab again and check if the query string is not stripped.
  networkPromise = observeChannel(TEST_URI, TEST_QUERY_STRING);
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testURI);
  await networkPromise;

  // Verify if the query string is not stripped because it's in the content
  // blocking allow list.
  await verifyQueryString(tab.linkedBrowser, TEST_QUERY_STRING);

  // Toggle ETP on and verify if the query string is stripped again.
  networkPromise = observeChannel(TEST_URI, "");
  browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    TEST_URI
  );
  gProtectionsHandler.enableForCurrentPage();
  await browserLoadedPromise;
  await networkPromise;

  await verifyQueryString(tab.linkedBrowser, "");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function doTestsForWindowOpen() {
  let testURI = TEST_THIRD_PARTY_URI + "?" + TEST_QUERY_STRING;

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Observe the channel and check if the query string is stripped.
    let networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");

    // Create the promise to wait for the opened tab.
    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url => {
      return url.startsWith(TEST_THIRD_PARTY_URI);
    });

    // Call window.open() to open the third-party URI.
    await SpecialPowers.spawn(browser, [testURI], async url => {
      content.postMessage({ type: "window-open", url }, "*");
    });

    await networkPromise;
    let newTab = await newTabPromise;

    // Verify if the query string is stripped in the new opened tab.
    await verifyQueryString(newTab.linkedBrowser, "");

    // Toggle ETP off and verify if the query string is restored.
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      newTab.linkedBrowser,
      false,
      testURI
    );
    // Observe the channel and check if the query string is not stripped.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, TEST_QUERY_STRING);

    gProtectionsHandler.disableForCurrentPage();
    await browserLoadedPromise;
    await networkPromise;

    await verifyQueryString(newTab.linkedBrowser, TEST_QUERY_STRING);

    BrowserTestUtils.removeTab(newTab);

    // Call window.open() again to check if the query string is not stripped if
    // it's in the content blocking allow list.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, TEST_QUERY_STRING);
    newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url => {
      return url.startsWith(TEST_THIRD_PARTY_URI);
    });

    await SpecialPowers.spawn(browser, [testURI], async url => {
      content.postMessage({ type: "window-open", url }, "*");
    });

    await networkPromise;
    newTab = await newTabPromise;

    // Verify if the query string is not stripped in the new opened tab.
    await verifyQueryString(newTab.linkedBrowser, TEST_QUERY_STRING);

    // Toggle ETP on and verify if the query string is stripped again.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");
    browserLoadedPromise = BrowserTestUtils.browserLoaded(
      newTab.linkedBrowser,
      false,
      TEST_THIRD_PARTY_URI
    );
    gProtectionsHandler.enableForCurrentPage();
    await browserLoadedPromise;
    await networkPromise;

    await verifyQueryString(newTab.linkedBrowser, "");
    BrowserTestUtils.removeTab(newTab);
  });
});

add_task(async function doTestsForLinkClick() {
  let testURI = TEST_THIRD_PARTY_URI + "?" + TEST_QUERY_STRING;

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Observe the channel and check if the query string is stripped.
    let networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");

    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_THIRD_PARTY_URI
    );

    // Create a link and click it to navigate.
    await SpecialPowers.spawn(browser, [testURI], async uri => {
      let link = content.document.createElement("a");
      link.setAttribute("href", uri);
      link.textContent = "Link";
      content.document.body.appendChild(link);
      link.click();
    });

    await networkPromise;
    await locationChangePromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, "");

    // Toggle ETP off and verify if the query string is restored.
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      testURI
    );
    // Observe the channel and check if the query string is not stripped.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, TEST_QUERY_STRING);

    gProtectionsHandler.disableForCurrentPage();
    await browserLoadedPromise;
    await networkPromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, TEST_QUERY_STRING);
  });

  // Repeat the test again to see if the query string is not stripped if it's in
  // the content blocking allow list.
  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Observe the channel and check if the query string is not stripped.
    let networkPromise = observeChannel(
      TEST_THIRD_PARTY_URI,
      TEST_QUERY_STRING
    );

    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      testURI
    );

    // Create a link and click it to navigate.
    await SpecialPowers.spawn(browser, [testURI], async uri => {
      let link = content.document.createElement("a");
      link.setAttribute("href", uri);
      link.textContent = "Link";
      content.document.body.appendChild(link);
      link.click();
    });

    await networkPromise;
    await locationChangePromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, TEST_QUERY_STRING);

    // Toggle ETP on and verify if the query string is stripped again.
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      TEST_THIRD_PARTY_URI
    );
    // Observe the channel and check if the query string is not stripped.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");

    gProtectionsHandler.enableForCurrentPage();
    await browserLoadedPromise;
    await networkPromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, "");
  });
});

add_task(async function doTestsForScriptNavigation() {
  let testURI = TEST_THIRD_PARTY_URI + "?" + TEST_QUERY_STRING;

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Observe the channel and check if the query string is stripped.
    let networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");

    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_THIRD_PARTY_URI
    );

    // Trigger the navigation by script.
    await SpecialPowers.spawn(browser, [testURI], async url => {
      content.postMessage({ type: "script", url }, "*");
    });

    await networkPromise;
    await locationChangePromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, "");

    // Toggle ETP off and verify if the query string is restored.
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      testURI
    );
    // Observe the channel and check if the query string is not stripped.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, TEST_QUERY_STRING);

    gProtectionsHandler.disableForCurrentPage();
    await browserLoadedPromise;
    await networkPromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, TEST_QUERY_STRING);
  });

  // Repeat the test again to see if the query string is not stripped if it's in
  // the content blocking allow list.
  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Observe the channel and check if the query string is not stripped.
    let networkPromise = observeChannel(
      TEST_THIRD_PARTY_URI,
      TEST_QUERY_STRING
    );

    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      testURI
    );

    // Trigger the navigation by script.
    await SpecialPowers.spawn(browser, [testURI], async url => {
      content.postMessage({ type: "script", url }, "*");
    });

    await networkPromise;
    await locationChangePromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, TEST_QUERY_STRING);

    // Toggle ETP on and verify if the query string is stripped again.
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      TEST_THIRD_PARTY_URI
    );
    // Observe the channel and check if the query string is stripped.
    networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");

    gProtectionsHandler.enableForCurrentPage();
    await browserLoadedPromise;
    await networkPromise;

    // Verify the query string in the content window.
    await verifyQueryString(browser, "");
  });
});

add_task(async function doTestsForRedirect() {
  let testURI = `${TEST_REDIRECT_URI}?${TEST_THIRD_PARTY_URI}?${TEST_QUERY_STRING}`;
  let resultURI = TEST_THIRD_PARTY_URI;
  let resultURIWithQuery = `${TEST_THIRD_PARTY_URI}?${TEST_QUERY_STRING}`;

  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  // Observe the channel and check if the query string is stripped.
  let networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");

  // Create the promise to wait for the location change.
  let locationChangePromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    resultURI
  );

  // Trigger the redirect.
  await SpecialPowers.spawn(tab.linkedBrowser, [testURI], async url => {
    content.postMessage({ type: "script", url }, "*");
  });

  await networkPromise;
  await locationChangePromise;

  // Verify the query string in the content window.
  await verifyQueryString(tab.linkedBrowser, "");

  // Toggle ETP off and verify if the query string is restored.
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    resultURIWithQuery
  );
  // Observe the channel and check if the query string is not stripped.
  networkPromise = observeChannel(TEST_THIRD_PARTY_URI, TEST_QUERY_STRING);

  gProtectionsHandler.disableForCurrentPage();
  await browserLoadedPromise;
  await networkPromise;

  BrowserTestUtils.removeTab(tab);

  // Open the tab again to check if the query string is not stripped.
  networkPromise = observeChannel(TEST_THIRD_PARTY_URI, TEST_QUERY_STRING);
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  locationChangePromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    resultURIWithQuery
  );

  // Trigger the redirect.
  await SpecialPowers.spawn(tab.linkedBrowser, [testURI], async url => {
    content.postMessage({ type: "script", url }, "*");
  });

  await networkPromise;
  await locationChangePromise;

  // Verify the query string in the content window.
  await verifyQueryString(tab.linkedBrowser, TEST_QUERY_STRING);

  // Toggle ETP on and verify if the query string is stripped again.
  networkPromise = observeChannel(TEST_THIRD_PARTY_URI, "");
  browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    resultURI
  );
  gProtectionsHandler.enableForCurrentPage();
  await browserLoadedPromise;
  await networkPromise;

  await verifyQueryString(tab.linkedBrowser, "");

  BrowserTestUtils.removeTab(tab);
});
