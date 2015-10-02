var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

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
function WebRequestEventManager(context, eventName)
{
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
        url: data.url,
        method: data.method,
        type: data.type,
        timeStamp: Date.now(),
        frameId: ExtensionManagement.getFrameId(data.windowId),
        parentFrameId: ExtensionManagement.getParentFrameId(data.parentWindowId, data.windowId),
      };

      // Fills in tabId typically.
      let result = {};
      extensions.emit("fill-browser-data", data.browser, data2, result);
      if (result.cancel) {
        return;
      }

      let optional = ["requestHeaders", "responseHeaders", "statusCode"];
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

extensions.registerPrivilegedAPI("webRequest", (extension, context) => {
  return {
    webRequest: {
      ResourceType: {
        MAIN_FRAME: "main_frame",
        SUB_FRAME: "sub_frame",
        STYLESHEET: "stylesheet",
        SCRIPT: "script",
        IMAGE: "image",
        OBJECT: "object",
        OBJECT_SUBREQUEST: "object_subrequest",
        XMLHTTPREQUEST: "xmlhttprequest",
        XBL: "xbl",
        XSLT: "xslt",
        PING: "ping",
        BEACON: "beacon",
        XML_DTD: "xml_dtd",
        FONT: "font",
        MEDIA: "media",
        WEBSOCKET: "websocket",
        CSP_REPORT: "csp_report",
        IMAGESET: "imageset",
        WEB_MANIFEST: "web_manifest",
        OTHER: "other",
      },

      onBeforeRequest: new WebRequestEventManager(context, "onBeforeRequest").api(),
      onBeforeSendHeaders: new WebRequestEventManager(context, "onBeforeSendHeaders").api(),
      onSendHeaders: new WebRequestEventManager(context, "onSendHeaders").api(),
      onHeadersReceived: new WebRequestEventManager(context, "onHeadersReceived").api(),
      onResponseStarted: new WebRequestEventManager(context, "onResponseStarted").api(),
      onCompleted: new WebRequestEventManager(context, "onCompleted").api(),
      handlerBehaviorChanged: function() {
        // TODO: Flush all caches.
      },
    },
  };
});
