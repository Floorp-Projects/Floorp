/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const COOKIES_CHECK_TIMEOUT_MS = 1000;

function sendCookies(cookies) {
    let message = {
     'url': document.location.href,
     'cookies': cookies
    }
    browser.runtime.sendNativeMessage("BrowserCookiesMessage", message);
}

function notify(message) {
   sendCookies(message.cookies);
}

browser.runtime.onMessage.addListener(notify);

const events = ["pageshow", "load", "unload"];
var timeout;

const eventLogger = event => {
  switch (event.type) {
    case "load":
        timeout = setTimeout(() => {
            browser.runtime.sendMessage({"checkCookies": true});
        }, COOKIES_CHECK_TIMEOUT_MS);
        break;
    case "pageshow":
        if (event.persisted) {
          timeout = setTimeout(() => {
              browser.runtime.sendMessage({"checkCookies": true});
          }, COOKIES_CHECK_TIMEOUT_MS);
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
