/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This background script is needed to update the current tab
// and activate reader view.

browser.runtime.onMessage.addListener(message => {
  switch (message.action) {
     case 'addSerializedDoc':
        browser.storage.session.set({ [message.id]: message.doc });
        return Promise.resolve();
     case 'getSerializedDoc':
        return (async () => {
          let doc = await browser.storage.session.get(message.id);
          browser.storage.session.remove(message.id);
          return doc[message.id];
        })();
     default:
       console.error(`Received unsupported action ${message.action}`);
   }
});
