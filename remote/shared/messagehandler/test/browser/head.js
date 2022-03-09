/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ContextDescriptorType } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandler.jsm"
);

var contextDescriptorAll = {
  type: ContextDescriptorType.All,
};

function createRootMessageHandler(sessionId) {
  const { RootMessageHandlerRegistry } = ChromeUtils.import(
    "chrome://remote/content/shared/messagehandler/RootMessageHandlerRegistry.jsm"
  );
  return RootMessageHandlerRegistry.getOrCreateMessageHandler(sessionId);
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
    `https://${domain}/document-builder.sjs?html=frame-${domain}`
  );
}

function createFrameForUri(uri) {
  return `<iframe src="${encodeURI(uri)}"></iframe>`;
}

/**
 * Create a XUL browser element in the provided XUL tab, with the provided type.
 *
 * @param {xul:tab} tab
 *     The XUL tab in which the browser element should be inserted.
 * @param {String} type
 *     The type attribute of the browser element, "chrome" or "content".
 * @return {xul:browser}
 *     The created browser element.
 */
function createParentBrowserElement(tab, type) {
  const parentBrowser = gBrowser.ownerDocument.createXULElement("browser");
  parentBrowser.setAttribute("type", type);
  const container = gBrowser.getBrowserContainer(tab.linkedBrowser);
  container.appendChild(parentBrowser);

  return parentBrowser;
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
 * Install a sidebar extension.
 *
 * @return {Object}
 *     Return value with two properties:
 *     - extension: test wrapper as returned by SpecialPowers.loadExtension.
 *       Make sure to explicitly call extension.unload() before the end of the test.
 *     - sidebarBrowser: the browser element containing the extension sidebar.
 */
async function installSidebarExtension() {
  info("Load the test extension");
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    useAddonManager: "temporary",

    files: {
      "sidebar.html": `
        <!DOCTYPE html>
        <html>
          Test extension
          <script src="sidebar.js"></script>
        </html>
      `,
      "sidebar.js": function() {
        const { browser } = this;
        browser.test.sendMessage("sidebar-loaded", {
          bcId: SpecialPowers.wrap(window).browsingContext.id,
        });
      },
      "tab.html": `
        <!DOCTYPE html>
        <html>
          Test extension (tab)
          <script src="tab.js"></script>
        </html>
      `,
      "tab.js": function() {
        const { browser } = this;
        browser.test.sendMessage("tab-loaded", {
          bcId: SpecialPowers.wrap(window).browsingContext.id,
        });
      },
    },
  });

  info("Wait for the extension to start");
  await extension.startup();

  info("Wait for the extension browsing context");
  const { bcId } = await extension.awaitMessage("sidebar-loaded");
  const sidebarBrowser = BrowsingContext.get(bcId).top.embedderElement;
  ok(sidebarBrowser, "Got a browser element for the extension sidebar");

  return {
    extension,
    sidebarBrowser,
  };
}
