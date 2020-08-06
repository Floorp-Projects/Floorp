/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This script is injected into content to determine whether or not a
// page is readerable, and to open a reader view extension page via
// the background script.

const supportedProtocols = ["http:", "https:"];

// Prevent false positives for these sites. This list is taken from Fennec:
// https://dxr.mozilla.org/mozilla-central/rev/7d47e7fa2489550ffa83aae67715c5497048923f/toolkit/components/reader/Readerable.js#45
const blockedHosts = [
  "amazon.com",
  "github.com",
  "mail.google.com",
  "pinterest.com",
  "reddit.com",
  "twitter.com",
  "youtube.com"
];

function isReaderable() {
    if (!supportedProtocols.includes(location.protocol)) {
      return false;
    }

    if (blockedHosts.some(blockedHost => location.hostname.endsWith(blockedHost))) {
      return false;
    }

    if (location.pathname == "/") {
      return false;
    }

    return isProbablyReaderable(document, _isNodeVisible);
}

function _isNodeVisible(node) {
    return node.clientHeight > 0 && node.clientWidth > 0;
}

function connectNativePort() {
  let port = browser.runtime.connectNative("mozacReaderview");
  port.onMessage.addListener((message) => {
     switch (message.action) {
       case 'show':
         browser.runtime.sendMessage({action: "show", options: message.value, url: location.href});

         let serializedDoc = new XMLSerializer().serializeToString(document);
         browser.runtime.sendMessage({action: "addSerializedDoc", doc: serializedDoc});
         break;
       case 'checkReaderState':
         port.postMessage({baseUrl: browser.runtime.getURL("/"), readerable: isReaderable()});
         break;
       default:
         console.error(`Received unsupported action ${message.action}`);
     }
  });

  return port;
}

let port = connectNativePort();

// When navigating to a cached page, this content script won't run again, but we
// do want to connect a new native port to trigger a new readerable check and
// apply the same logic (as on page load) in our feature class (e.g. updating the
// toolbar etc.)
window.addEventListener("pageshow", (event) => {
  port = (port != null)? port : connectNativePort();
});

window.addEventListener("pagehide", (event) => {
  port.disconnect();
  port = null;
});
