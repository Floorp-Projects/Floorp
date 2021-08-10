/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function createRootMessageHandler(sessionId) {
  const { MessageHandlerRegistry } = ChromeUtils.import(
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm"
  );
  const { RootMessageHandler } = ChromeUtils.import(
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
  );
  return MessageHandlerRegistry.getOrCreateMessageHandler(
    sessionId,
    RootMessageHandler.type
  );
}

/**
 * Load the provided url in an existing browser.
 * Returns a promise which will resolve when the page is loaded.
 *
 * @param {Browser} browser
 *     The browser element where the URL should be loaded.
 * @param {String} url
 *     The URL to load in the new tab
 */
async function loadURL(browser, url) {
  const loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.loadURI(browser, url);
  return loaded;
}

/**
 * Create a new foreground tab loading the provided url.
 * Returns a promise which will resolve when the page is loaded.
 *
 * @param {String} url
 *     The URL to load in the new tab
 */
async function addTab(url) {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  registerCleanupFunction(() => {
    gBrowser.removeTab(tab);
  });
  return tab;
}

/**
 * Create inline markup for a simple iframe that can be used with
 * document-builder.sjs. The iframe will be served under the provided domain.
 *
 * @param {String} domain
 *     A domain (eg "example.com"), compatible with build/pgo/server-locations.txt
 */
function createFrame(domain) {
  return createFrameForUri(
    `http://${domain}/document-builder.sjs?html=frame-${domain}`
  );
}

function createFrameForUri(uri) {
  return `<iframe src="${encodeURI(uri)}"></iframe>`;
}

// Create a test page with 2 iframes:
// - one with a different eTLD+1 (example.com)
// - one with a nested iframe on a different eTLD+1 (example.net)
//
// Overall the document structure should look like:
//
// html (example.org)
//   iframe (example.org)
//     iframe (example.net)
//   iframe(example.com)
//
// Which means we should have 4 browsing contexts in total.
function createTestMarkupWithFrames() {
  // Create the markup for an example.net frame nested in an example.com frame.
  const NESTED_FRAME_MARKUP = createFrameForUri(
    `http://example.org/document-builder.sjs?html=${createFrame("example.net")}`
  );

  // Combine the nested frame markup created above with an example.com frame.
  const TEST_URI_MARKUP = `${NESTED_FRAME_MARKUP}${createFrame("example.com")}`;

  // Create the test page URI on example.org.
  return `http://example.org/document-builder.sjs?html=${encodeURI(
    TEST_URI_MARKUP
  )}`;
}
