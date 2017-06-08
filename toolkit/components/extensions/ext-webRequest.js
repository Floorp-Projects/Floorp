"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

// This file expectes tabTracker to be defined in the global scope (e.g.
// by ext-utils.js).
/* global tabTracker */

XPCOMUtils.defineLazyModuleGetter(this, "WebRequest",
                                  "resource://gre/modules/WebRequest.jsm");

// EventManager-like class specifically for WebRequest. Inherits from
// SingletonEventManager. Takes care of converting |details| parameter
// when invoking listeners.
function WebRequestEventManager(context, eventName) {
  let name = `webRequest.${eventName}`;
  let register = (fire, filter, info) => {
    let listener = data => {
      // Prevent listening in on requests originating from system principal to
      // prevent tinkering with OCSP, app and addon updates, etc.
      if (data.isSystemPrincipal) {
        return;
      }

      // Check hosts permissions for both the resource being requested,
      const hosts = context.extension.whiteListedHosts;
      if (!hosts.matches(Services.io.newURI(data.url))) {
        return;
      }
      // and the origin that is loading the resource.
      const origin = data.documentUrl;
      const own = origin && origin.startsWith(context.extension.getURL());
      if (origin && !own && !hosts.matches(Services.io.newURI(origin))) {
        return;
      }

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

      let data2 = {
        requestId: data.requestId,
        url: data.url,
        originUrl: data.originUrl,
        documentUrl: data.documentUrl,
        method: data.method,
        tabId: browserData.tabId,
        type: data.type,
        timeStamp: Date.now(),
        frameId: data.windowId,
        parentFrameId: data.parentWindowId,
      };

      const maybeCached = ["onResponseStarted", "onBeforeRedirect", "onCompleted", "onErrorOccurred"];
      if (maybeCached.includes(eventName)) {
        data2.fromCache = !!data.fromCache;
      }

      if ("ip" in data) {
        data2.ip = data.ip;
      }

      let optional = ["requestHeaders", "responseHeaders", "statusCode", "statusLine", "error", "redirectUrl",
                      "requestBody", "scheme", "realm", "isProxy", "challenger"];
      for (let opt of optional) {
        if (opt in data) {
          data2[opt] = data[opt];
        }
      }

      return fire.sync(data2);
    };

    let filter2 = {};
    if (filter.urls) {
      let perms = new MatchPatternSet([...context.extension.whiteListedHosts.patterns,
                                       ...context.extension.optionalOrigins.patterns]);

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

    let info2 = [];
    if (info) {
      for (let desc of info) {
        if (desc == "blocking" && !context.extension.hasPermission("webRequestBlocking")) {
          Cu.reportError("Using webRequest.addListener with the blocking option " +
                         "requires the 'webRequestBlocking' permission.");
        } else {
          info2.push(desc);
        }
      }
    }

    WebRequest[eventName].addListener(listener, filter2, info2);
    return () => {
      WebRequest[eventName].removeListener(listener);
    };
  };

  return SingletonEventManager.call(this, context, name, register);
}

WebRequestEventManager.prototype = Object.create(SingletonEventManager.prototype);

this.webRequest = class extends ExtensionAPI {
  getAPI(context) {
    return {
      webRequest: {
        onBeforeRequest: new WebRequestEventManager(context, "onBeforeRequest").api(),
        onBeforeSendHeaders: new WebRequestEventManager(context, "onBeforeSendHeaders").api(),
        onSendHeaders: new WebRequestEventManager(context, "onSendHeaders").api(),
        onHeadersReceived: new WebRequestEventManager(context, "onHeadersReceived").api(),
        onAuthRequired: new WebRequestEventManager(context, "onAuthRequired").api(),
        onBeforeRedirect: new WebRequestEventManager(context, "onBeforeRedirect").api(),
        onResponseStarted: new WebRequestEventManager(context, "onResponseStarted").api(),
        onErrorOccurred: new WebRequestEventManager(context, "onErrorOccurred").api(),
        onCompleted: new WebRequestEventManager(context, "onCompleted").api(),
        handlerBehaviorChanged: function() {
          // TODO: Flush all caches.
        },
      },
    };
  }
};
