/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Add a new tab in a given browser, pointing to a given URL and automatically
 * register the cleanup function to remove it at the end of the test.
 *
 * @param {Browser} browser
 *     The browser element where the tab should be added.
 * @param {string} url
 *     The URL for the tab.
 * @param {object=} options
 *     Options object to forward to BrowserTestUtils.addTab.
 * @returns {Tab}
 *     The created tab.
 */
function addTab(browser, url, options) {
  const tab = BrowserTestUtils.addTab(browser, url, options);
  registerCleanupFunction(() => browser.removeTab(tab));
  return tab;
}

/**
 * Check if a given navigation is valid and has the expected url.
 *
 * @param {object} navigation
 *     The navigation to validate.
 * @param {string} expectedUrl
 *     The expected url for the navigation.
 */
function assertNavigation(navigation, expectedUrl) {
  ok(!!navigation, "Retrieved a navigation");
  is(navigation.url, expectedUrl, "Navigation has the expected URL");
  is(
    typeof navigation.navigationId,
    "string",
    "Navigation has a string navigationId"
  );
}

/**
 * Check a pair of navigation events have the expected URL, navigation id and
 * navigable id. The pair is expected to be ordered as follows: navigation-started
 * and then navigation-stopped.
 *
 * @param {Array<object>} events
 *     The pair of events to validate.
 * @param {string} url
 *     The expected url for the navigation.
 * @param {string} navigationId
 *     The expected navigation id.
 * @param {string} navigableId
 *     The expected navigable id.
 * @param {boolean} isSameDocument
 *     If the navigation should be a same document navigation.
 */
function assertNavigationEvents(
  events,
  url,
  navigationId,
  navigableId,
  isSameDocument
) {
  const expectedEvents = isSameDocument ? 1 : 2;

  const navigationEvents = events.filter(
    e => e.data.navigationId == navigationId
  );
  is(
    navigationEvents.length,
    expectedEvents,
    `Found ${expectedEvents} events for navigationId ${navigationId}`
  );

  if (isSameDocument) {
    // Check there are no navigation-started/stopped events.
    ok(!navigationEvents.some(e => e.name === "navigation-started"));
    ok(!navigationEvents.some(e => e.name === "navigation-stopped"));

    const locationChanged = navigationEvents.find(
      e => e.name === "location-changed"
    );
    is(locationChanged.name, "location-changed", "event has the expected name");
    is(locationChanged.data.url, url, "event has the expected url");
    is(
      locationChanged.data.navigableId,
      navigableId,
      "event has the expected navigable"
    );
  } else {
    // Check there is no location-changed event.
    ok(!navigationEvents.some(e => e.name === "location-changed"));

    const started = navigationEvents.find(e => e.name === "navigation-started");
    const stopped = navigationEvents.find(e => e.name === "navigation-stopped");

    // Check navigation-started
    is(started.name, "navigation-started", "event has the expected name");
    is(started.data.url, url, "event has the expected url");
    is(
      started.data.navigableId,
      navigableId,
      "event has the expected navigable"
    );

    // Check navigation-stopped
    is(stopped.name, "navigation-stopped", "event has the expected name");
    is(stopped.data.url, url, "event has the expected url");
    is(
      stopped.data.navigableId,
      navigableId,
      "event has the expected navigable"
    );
  }
}

/**
 * Assert that the given navigations all have unique/different ids.
 *
 * @param {Array<object>} navigations
 *     The navigations to validate.
 */
function assertUniqueNavigationIds(...navigations) {
  const ids = navigations.map(navigation => navigation.navigationId);
  is(new Set(ids).size, ids.length, "Navigation ids are all different");
}

/**
 * Create a document-builder based page with an iframe served by a given domain.
 *
 * @param {string} domain
 *     The domain which should serve the page.
 * @returns {string}
 *     The URI for the page.
 */
function createFrame(domain) {
  return createFrameForUri(
    `https://${domain}/document-builder.sjs?html=frame-${domain}`
  );
}

/**
 * Create the markup for an iframe pointing to a given URI.
 *
 * @param {string} uri
 *     The uri for the iframe.
 * @returns {string}
 *     The iframe markup.
 */
function createFrameForUri(uri) {
  return `<iframe src="${encodeURI(uri)}"></iframe>`;
}

/**
 * Create the URL for a test page containing nested iframes
 *
 * @returns {string}
 *     The test page url.
 */
function createTestPageWithFrames() {
  // Create the markup for an example.net frame nested in an example.com frame.
  const NESTED_FRAME_MARKUP = createFrameForUri(
    `https://example.org/document-builder.sjs?html=${createFrame(
      "example.net"
    )}`
  );

  // Combine the nested frame markup created above with an example.com frame.
  const TEST_URI_MARKUP = `${NESTED_FRAME_MARKUP}${createFrame("example.com")}`;

  // Create the test page URI on example.org.
  return `https://example.org/document-builder.sjs?html=${encodeURI(
    TEST_URI_MARKUP
  )}`;
}

/**
 * Load the provided url in an existing browser.
 *
 * @param {Browser} browser
 *     The browser element where the URL should be loaded.
 * @param {string} url
 *     The URL to load.
 * @param {object=} options
 * @param {boolean} options.includeSubFrames
 *     Whether we should monitor load of sub frames. Defaults to false.
 * @param {boolean} options.maybeErrorPage
 *     Whether we might reach an error page or not. Defaults to false.
 * @returns {Promise}
 *     Promise which will resolve when the page is loaded with the expected url.
 */
async function loadURL(browser, url, options = {}) {
  const { includeSubFrames = false, maybeErrorPage = false } = options;
  const loaded = BrowserTestUtils.browserLoaded(
    browser,
    includeSubFrames,
    url,
    maybeErrorPage
  );
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loaded;
}
