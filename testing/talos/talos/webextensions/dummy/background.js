"use strict";

/* eslint-env webextensions */

browser.webRequest.onBeforeRequest.addListener(
  details => {
    let filter = browser.webRequest.filterResponseData(details.requestId);

    filter.onstop = event => {
      filter.close();
    };
    filter.ondata = event => {
      filter.write(event.data);
    };
  }, {
    urls: ["<all_urls>"],
  },
  ["blocking"]);

browser.webRequest.onBeforeSendHeaders.addListener(
  details => {
    return {requestHeaders: details.requestHeaders};
  },
  {urls: ["https://*/*", "http://*/*"]},
  ["blocking", "requestHeaders"]);

browser.webRequest.onHeadersReceived.addListener(
  details => {
    return {responseHeaders: details.responseHeaders};
  },
  {urls: ["https://*/*", "http://*/*"]},
  ["blocking", "responseHeaders"]);

browser.webRequest.onErrorOccurred.addListener(
  details => {
  },
  {urls: ["https://*/*", "http://*/*"]});


browser.tabs.onUpdated.addListener((tabId, changed, tab) => {
});

browser.tabs.onActivated.addListener(({tabId, windowId}) => {
});

browser.tabs.onCreated.addListener(tab => {
});

browser.tabs.onRemoved.addListener((tabId, removeInfo) => {
});

browser.tabs.onAttached.addListener((tabId, attachInfo) => {
});

browser.tabs.onDetached.addListener((tabId, detachInfo) => {
});
