var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigation",
                                  "resource://gre/modules/WebNavigation.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  SingletonEventManager,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

// Similar to WebRequestEventManager but for WebNavigation.
function WebNavigationEventManager(context, eventName)
{
  let name = `webNavigation.${eventName}`;
  let register = callback => {
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

      runSafe(context, callback, data2);
    };

    WebNavigation[eventName].addListener(listener);
    return () => {
      WebNavigation[eventName].removeListener(listener);
    };
  };

  return SingletonEventManager.call(this, context, name, register);
}

WebNavigationEventManager.prototype = Object.create(SingletonEventManager.prototype);

extensions.registerPrivilegedAPI("webNavigation", (extension, context) => {
  return {
    webNavigation: {
      onBeforeNavigate: new WebNavigationEventManager(context, "onBeforeNavigate").api(),
      onCommitted: new WebNavigationEventManager(context, "onCommitted").api(),
      onDOMContentLoaded: new WebNavigationEventManager(context, "onDOMContentLoaded").api(),
      onCompleted: new WebNavigationEventManager(context, "onCompleted").api(),
      onErrorOccurred: new WebNavigationEventManager(context, "onErrorOccurred").api(),
      onReferenceFragmentUpdated: new WebNavigationEventManager(context, "onReferenceFragmentUpdated").api(),
      onCreatedNavigationTarget: ignoreEvent(),
    },
  };
});
