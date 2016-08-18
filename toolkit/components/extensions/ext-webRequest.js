"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebRequest",
                                  "resource://gre/modules/WebRequest.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  SingletonEventManager,
  runSafeSync,
} = ExtensionUtils;

// EventManager-like class specifically for WebRequest. Inherits from
// SingletonEventManager. Takes care of converting |details| parameter
// when invoking listeners.
function WebRequestEventManager(context, eventName) {
  let name = `webRequest.${eventName}`;
  let register = (callback, filter, info) => {
    let listener = data => {
      if (!data.browser) {
        return;
      }

      let tabId = TabManager.getBrowserId(data.browser);
      if (tabId == -1) {
        return;
      }

      let data2 = {
        requestId: data.requestId,
        url: data.url,
        originUrl: data.originUrl,
        method: data.method,
        type: data.type,
        timeStamp: Date.now(),
        frameId: ExtensionManagement.getFrameId(data.windowId),
        parentFrameId: ExtensionManagement.getParentFrameId(data.parentWindowId, data.windowId),
      };

      if ("ip" in data) {
        data2.ip = data.ip;
      }

      // Fills in tabId typically.
      let result = {};
      extensions.emit("fill-browser-data", data.browser, data2, result);
      if (result.cancel) {
        return;
      }

      let optional = ["requestHeaders", "responseHeaders", "statusCode", "statusLine", "error", "redirectUrl",
                      "requestBody"];
      for (let opt of optional) {
        if (opt in data) {
          data2[opt] = data[opt];
        }
      }

      return runSafeSync(context, callback, data2);
    };

    let filter2 = {};
    filter2.urls = new MatchPattern(filter.urls);
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

extensions.registerSchemaAPI("webRequest", (extension, context) => {
  return {
    webRequest: {
      onBeforeRequest: new WebRequestEventManager(context, "onBeforeRequest").api(),
      onBeforeSendHeaders: new WebRequestEventManager(context, "onBeforeSendHeaders").api(),
      onSendHeaders: new WebRequestEventManager(context, "onSendHeaders").api(),
      onHeadersReceived: new WebRequestEventManager(context, "onHeadersReceived").api(),
      onBeforeRedirect: new WebRequestEventManager(context, "onBeforeRedirect").api(),
      onResponseStarted: new WebRequestEventManager(context, "onResponseStarted").api(),
      onErrorOccurred: new WebRequestEventManager(context, "onErrorOccurred").api(),
      onCompleted: new WebRequestEventManager(context, "onCompleted").api(),
      handlerBehaviorChanged: function() {
        // TODO: Flush all caches.
      },
    },
  };
});
