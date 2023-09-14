/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function clearConsole() {
  for (const tab of gBrowser.tabs) {
    await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      Services.console.reset();
    });
  }
  Services.console.reset();
}

/**
 * Execute the provided script content by generating a dynamic script tag and
 * inserting it in the page for the current selected browser.
 *
 * @param {string} script
 *     The script to execute.
 * @returns {Promise}
 *     A promise that resolves when the script node was added and removed from
 *     the content page.
 */
function createScriptNode(script) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [script],
    function (_script) {
      var script = content.document.createElement("script");
      script.append(content.document.createTextNode(_script));
      content.document.body.append(script);
    }
  );
}

registerCleanupFunction(async () => {
  await clearConsole();
});

async function doGC() {
  // Run GC and CC a few times to make sure that as much as possible is freed.
  const numCycles = 3;
  for (let i = 0; i < numCycles; i++) {
    Cu.forceGC();
    Cu.forceCC();
    await new Promise(resolve => Cu.schedulePreciseShrinkingGC(resolve));
  }

  const MemoryReporter = Cc[
    "@mozilla.org/memory-reporter-manager;1"
  ].getService(Ci.nsIMemoryReporterManager);
  await new Promise(resolve => MemoryReporter.minimizeMemoryUsage(resolve));
}

/**
 * Load the provided url in an existing browser.
 * Returns a promise which will resolve when the page is loaded.
 *
 * @param {Browser} browser
 *     The browser element where the URL should be loaded.
 * @param {string} url
 *     The URL to load.
 */
async function loadURL(browser, url) {
  const loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loaded;
}

/**
 * Create a fetch request to `url` from the content page loaded in the provided
 * `browser`.
 *
 *
 * @param {Browser} browser
 *     The browser element where the fetch should be performed.
 * @param {string} url
 *     The URL to fetch.
 */
function fetch(browser, url) {
  return SpecialPowers.spawn(browser, [url], async _url => {
    const response = await content.fetch(_url);
    // Wait for response.text() to resolve as well to make sure the response
    // has completed before returning.
    await response.text();
  });
}
