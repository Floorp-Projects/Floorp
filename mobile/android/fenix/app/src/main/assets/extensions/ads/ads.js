/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ADLINK_CHECK_TIMEOUT_MS = 1000;

function collectLinks(urls) {
    let anchors = document.getElementsByTagName("a");
    for (let anchor of anchors) {
          if (!anchor.href) {
            continue;
          }
          urls.push(anchor.href);
    }
}

function sendLinks(cookies) {
    let urls = [];
    collectLinks(urls);

    let message = {
     'url': document.location.href,
     'urls': urls,
     'cookies': cookies
    };
    browser.runtime.sendNativeMessage("MozacBrowserAds", message);
}

function notify(message) {
   sendLinks(message.cookies);
}

browser.runtime.onMessage.addListener(notify);

const events = ["pageshow", "load", "unload"];
var timeout;

const eventLogger = event => {
  switch (event.type) {
    case "load":
        timeout = setTimeout(() => {
            browser.runtime.sendMessage({ "checkCookies": true });
        }, ADLINK_CHECK_TIMEOUT_MS)
        break;
    case "pageshow":
        if (event.persisted) {
          timeout = setTimeout(() => {
              browser.runtime.sendMessage({ "checkCookies": true });
          }, ADLINK_CHECK_TIMEOUT_MS)
        }
        break;
    case "unload":
        clearTimeout(timeout);
    default:
        console.log('Event:', event.type);
  }
};

events.forEach(eventName =>
  window.addEventListener(eventName, eventLogger)
);
