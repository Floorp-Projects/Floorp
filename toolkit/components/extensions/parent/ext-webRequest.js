"use strict";

// This file expects tabTracker to be defined in the global scope (e.g.
// by ext-utils.js).
/* global tabTracker */

ChromeUtils.defineModuleGetter(this, "WebRequest",
                               "resource://gre/modules/WebRequest.jsm");

// The guts of a WebRequest event handler.  Takes care of converting
// |details| parameter when invoking listeners.
function registerEvent(extension, eventName, fire, filter, info, tabParent = null) {
  let listener = async data => {
    let browserData = {tabId: -1, windowId: -1};
    if (data.browser) {
      browserData = tabTracker.getBrowserData(data.browser);
    }
    if (filter.tabId != null && browserData.tabId != filter.tabId) {
      return;
    }
    if (filter.windowId != null && browserData.windowId != filter.windowId) {
      return;
    }

    let event = data.serialize(eventName);
    event.tabId = browserData.tabId;

    if (data.registerTraceableChannel) {
      if (fire.wakeup) {
        await fire.wakeup();
      }
      data.registerTraceableChannel(extension.policy, tabParent);
    }

    return fire.sync(event);
  };

  let filter2 = {};
  if (filter.urls) {
    let perms = new MatchPatternSet([...extension.whiteListedHosts.patterns,
                                     ...extension.optionalOrigins.patterns]);

    filter2.urls = new MatchPatternSet(filter.urls);

    if (!perms.overlapsAll(filter2.urls)) {
      Cu.reportError("The webRequest.addListener filter doesn't overlap with host permissions.");
    }
  }
  if (filter.types) {
    filter2.types = filter.types;
  }
  if (filter.tabId) {
    filter2.tabId = filter.tabId;
  }
  if (filter.windowId) {
    filter2.windowId = filter.windowId;
  }

  let blockingAllowed = extension.hasPermission("webRequestBlocking");

  let info2 = [];
  if (info) {
    for (let desc of info) {
      if (desc == "blocking" && !blockingAllowed) {
        Cu.reportError("Using webRequest.addListener with the blocking option " +
                       "requires the 'webRequestBlocking' permission.");
      } else {
        info2.push(desc);
      }
    }
  }

  let listenerDetails = {
    addonId: extension.id,
    extension: extension.policy,
    blockingAllowed,
  };

  WebRequest[eventName].addListener(
    listener, filter2, info2,
    listenerDetails);

  return {
    unregister: () => { WebRequest[eventName].removeListener(listener); },
    convert(_fire, context) {
      fire = _fire;
      tabParent = context.xulBrowser.frameLoader.tabParent;
    },
  };
}

function makeWebRequestEvent(context, name) {
  return new EventManager({
    context,
    name: `webRequest.${name}`,
    persistent: {
      module: "webRequest",
      event: name,
    },
    register: (fire, filter, info) => {
      return registerEvent(context.extension, name, fire, filter, info,
                           context.xulBrowser.frameLoader.tabParent).unregister;
    },
  }).api();
}

this.webRequest = class extends ExtensionAPI {
  primeListener(extension, event, fire, params) {
    return registerEvent(extension, event, fire, ...params);
  }

  getAPI(context) {
    return {
      webRequest: {
        onBeforeRequest: makeWebRequestEvent(context, "onBeforeRequest"),
        onBeforeSendHeaders: makeWebRequestEvent(context, "onBeforeSendHeaders"),
        onSendHeaders: makeWebRequestEvent(context, "onSendHeaders"),
        onHeadersReceived: makeWebRequestEvent(context, "onHeadersReceived"),
        onAuthRequired: makeWebRequestEvent(context, "onAuthRequired"),
        onBeforeRedirect: makeWebRequestEvent(context, "onBeforeRedirect"),
        onResponseStarted: makeWebRequestEvent(context, "onResponseStarted"),
        onErrorOccurred: makeWebRequestEvent(context, "onErrorOccurred"),
        onCompleted: makeWebRequestEvent(context, "onCompleted"),
        handlerBehaviorChanged: function() {
          // TODO: Flush all caches.
        },
      },
    };
  }
};
