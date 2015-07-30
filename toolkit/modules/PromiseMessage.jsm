/* This Source Code Form is subject to the terms of the Mozilla Public
+ * License, v. 2.0. If a copy of the MPL was not distributed with this
+ * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PromiseMessage"];

let msgId = 0;

let PromiseMessage = {
  send(messageManager, name, data = {}) {
    let id = msgId++;

    // Make a copy of data so that the caller doesn't see us setting 'id'.
    let dataCopy = {};
    for (let prop in data) {
      dataCopy[prop] = data[prop];
    }
    dataCopy.id = id;

    // Send the message.
    messageManager.sendAsyncMessage(name, dataCopy);

    // Return a promise that resolves when we get a reply (a message of the same name).
    return new Promise(resolve => {
      messageManager.addMessageListener(name, function listener(reply) {
        if (reply.data.id !== id) {
          return;
        }
        messageManager.removeMessageListener(name, listener);
        resolve(reply);
      });
    });
  }
};
