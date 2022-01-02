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
