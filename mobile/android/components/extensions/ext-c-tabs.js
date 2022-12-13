/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.tabs = class extends ExtensionAPI {
  getAPI(context) {
    return {
      tabs: {
        connect(tabId, options) {
          const { frameId = null, name = "" } = options || {};
          return context.messenger.connect({ name, tabId, frameId });
        },

        sendMessage(tabId, message, options, callback) {
          const arg = { tabId, frameId: options?.frameId, message, callback };
          return context.messenger.sendRuntimeMessage(arg);
        },
      },
    };
  }
};
