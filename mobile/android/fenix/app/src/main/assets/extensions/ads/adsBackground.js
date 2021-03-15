/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

browser.runtime.onMessage.addListener(notify);

function sendMessageToTabs(tabs, cookies) {
  for (let tab of tabs) {
    browser.tabs.sendMessage(
      tab.id,
      { cookies }
    );
  }
}

function notify(message) {
    if (message.checkCookies) {
        browser.cookies.getAll({})
            .then(cookies => {
                browser.tabs.query({
                    currentWindow: true,
                    active: true
                }).then(tabs => {
                    sendMessageToTabs(tabs, cookies);
                });
            });
    }
}
