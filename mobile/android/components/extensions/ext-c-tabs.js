/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

this.tabs = class extends ExtensionAPI {
  getAPI(context) {
    return {
      tabs: {
        connect(tabId, options) {
          const { frameId = null, name = "" } = options || {};
          return context.messenger.nm.connect({ name, tabId, frameId });
        },

        sendMessage: function(tabId, message, options, responseCallback) {
          const recipient = {
            extensionId: context.extension.id,
            tabId: tabId,
          };
          if (options && options.frameId !== null) {
            recipient.frameId = options.frameId;
          }
          return context.messenger.sendMessage(
            context.messageManager,
            message,
            recipient,
            responseCallback
          );
        },
      },
    };
  }
};
