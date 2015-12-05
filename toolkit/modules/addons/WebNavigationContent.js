"use strict";

/* globals docShell */

var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function getWindowId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils)
               .outerWindowID;
}

function getParentWindowId(window) {
  return getWindowId(window.parent);
}

function loadListener(event) {
  let document = event.target;
  let window = document.defaultView;
  let url = document.documentURI;
  let windowId = getWindowId(window);
  let parentWindowId = getParentWindowId(window);
  sendAsyncMessage("Extension:DOMContentLoaded", {windowId, parentWindowId, url});
}

addEventListener("DOMContentLoaded", loadListener);
addMessageListener("Extension:DisableWebNavigation", () => {
  removeEventListener("DOMContentLoaded", loadListener);
});

var WebProgressListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
                                          Ci.nsIWebProgress.NOTIFY_LOCATION);
  },

  uninit() {
    if (!docShell) {
      return;
    }
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);
  },

  onStateChange: function onStateChange(webProgress, request, stateFlags, status) {
    let data = {
      requestURL: request.QueryInterface(Ci.nsIChannel).URI.spec,
      windowId: webProgress.DOMWindowID,
      parentWindowId: getParentWindowId(webProgress.DOMWindow),
      status,
      stateFlags,
    };
    sendAsyncMessage("Extension:StateChange", data);

    if (webProgress.DOMWindow.top != webProgress.DOMWindow) {
      let webNav = webProgress.QueryInterface(Ci.nsIWebNavigation);
      if (!webNav.canGoBack) {
        // For some reason we don't fire onLocationChange for the
        // initial navigation of a sub-frame. So we need to simulate
        // it here.
        let data = {
          location: request.QueryInterface(Ci.nsIChannel).URI.spec,
          windowId: webProgress.DOMWindowID,
          parentWindowId: getParentWindowId(webProgress.DOMWindow),
          flags: 0,
        };
        sendAsyncMessage("Extension:LocationChange", data);
      }
    }
  },

  onLocationChange: function onLocationChange(webProgress, request, locationURI, flags) {
    let data = {
      location: locationURI ? locationURI.spec : "",
      windowId: webProgress.DOMWindowID,
      parentWindowId: getParentWindowId(webProgress.DOMWindow),
      flags,
    };
    sendAsyncMessage("Extension:LocationChange", data);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference]),
};

var disabled = false;
WebProgressListener.init();
addEventListener("unload", () => {
  if (!disabled) {
    WebProgressListener.uninit();
  }
});
addMessageListener("Extension:DisableWebNavigation", () => {
  disabled = true;
  WebProgressListener.uninit();
});
