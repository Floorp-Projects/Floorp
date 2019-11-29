/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Avoid adding ID selector rules in this style sheet, since they could
 * inadvertently match elements in the article content. */

// Counts to three and sends a greeting via the browser action of a newly created tab.
browser.tabs.onCreated.addListener((tab) => {
  let counter = 0;
  let intervalId = setInterval(() => {
    var message;
    if (++counter <= 3) {
      message = "" + counter;
    } else {
      message = "Hi!";
      clearInterval(intervalId);
    }
    browser.browserAction.setBadgeTextColor({tabId: tab.id, color: "#FFFFFF"});
    browser.browserAction.setBadgeText({tabId: tab.id, text: message});
  }, 1000);
});

browser.browserAction.setBadgeBackgroundColor({color: "#AAAAAA"});