/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var {
  ExtensionError,
} = ExtensionUtils;

function makeWebRequestEvent(context, name) {
  return new EventManager({
    context,
    name: `webRequest.${name}`,
    register: (fire, filter, info) => {
      const blockingAllowed = context.extension
        .hasPermission("webRequestBlocking");

      if (info) {
        // "blocking" requires webRequestBlocking permission
        for (let desc of info) {
          if (desc == "blocking" && !blockingAllowed) {
            throw new ExtensionError("Using webRequest.addListener with the " +
              "blocking option requires the 'webRequestBlocking' permission.");
          }
        }
      }

      const listener = (...args) => fire.async(...args);

      let parent = context.childManager.getParentEvent(`webRequest.${name}`);
      parent.addListener(listener, filter, info);
      return () => {
        parent.removeListener(listener);
      };
    },
  }).api();
}

this.webRequest = class extends ExtensionAPI {
  getAPI(context) {
    let filters = new WeakSet();

    context.callOnClose({
      close() {
        for (let filter of ChromeUtils.nondeterministicGetWeakSetKeys(filters)) {
          try {
            filter.disconnect();
          } catch (e) {
            // Ignore.
          }
        }
      },
    });

    return {
      webRequest: {
        filterResponseData(requestId) {
          requestId = parseInt(requestId, 10);

          let streamFilter = context.cloneScope.StreamFilter.create(
            requestId, context.extension.id);

          filters.add(streamFilter);
          return streamFilter;
        },
        onBeforeRequest: makeWebRequestEvent(context, "onBeforeRequest"),
        onBeforeSendHeaders:
          makeWebRequestEvent(context, "onBeforeSendHeaders"),
        onHeadersReceived: makeWebRequestEvent(context, "onHeadersReceived"),
        onAuthRequired: makeWebRequestEvent(context, "onAuthRequired"),
      },
    };
  }
};
