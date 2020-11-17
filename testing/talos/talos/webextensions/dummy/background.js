/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env webextensions */

let skipFilters = false;

browser.webRequest.onBeforeRequest.addListener(
  details => {
    if (details.url.endsWith("/startup_test/tspaint_test.html")) {
      skipFilters = true;
    }
    if (skipFilters || details.url.endsWith("/favicon.ico")) {
      return;
    }

    let filter = browser.webRequest.filterResponseData(details.requestId);

    filter.onstop = event => {
      filter.close();
    };
    filter.ondata = event => {
      filter.write(event.data);
    };
  },
  {
    urls: ["<all_urls>"],
  },
  ["blocking"]
);

browser.webRequest.onBeforeSendHeaders.addListener(
  details => {
    return { requestHeaders: details.requestHeaders };
  },
  { urls: ["https://*/*", "http://*/*"] },
  ["blocking", "requestHeaders"]
);

browser.webRequest.onHeadersReceived.addListener(
  details => {
    return { responseHeaders: details.responseHeaders };
  },
  { urls: ["https://*/*", "http://*/*"] },
  ["blocking", "responseHeaders"]
);

browser.webRequest.onErrorOccurred.addListener(details => {}, {
  urls: ["https://*/*", "http://*/*"],
});

browser.runtime.onMessage.addListener(msg => {
  return Promise.resolve({ code: "10-4", msg });
});

browser.tabs.onUpdated.addListener((tabId, changed, tab) => {
  if (changed.url) {
    browser.pageAction.show(tabId);
  }
  if (changed.title) {
    browser.pageAction.setTitle({ tabId, title: `title: ${tab.title}` });
    browser.pageAction.setIcon({ tabId, path: { 16: "/icon.png" } });

    browser.browserAction.setTitle({ tabId, title: `title: ${tab.title}` });
    browser.browserAction.setIcon({ path: { 16: "/icon.png" } });
  }

  browser.tabs.sendMessage(tabId, { changed, tab }).catch(() => {
    // Ignore tabs that don't have a listener yet.
  });
});

browser.tabs.onActivated.addListener(({ tabId, windowId }) => {
  browser.pageAction.show(tabId);
});

browser.tabs.onCreated.addListener(tab => {
  browser.pageAction.show(tab.id);
});

browser.tabs.onRemoved.addListener((tabId, removeInfo) => {});

browser.tabs.onAttached.addListener((tabId, attachInfo) => {});

browser.tabs.onDetached.addListener((tabId, detachInfo) => {});
