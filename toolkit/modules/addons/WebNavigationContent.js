"use strict";

/* globals docShell */

var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

function loadListener(event) {
  let document = event.target;
  let window = document.defaultView;
  let url = document.documentURI;
  let windowId = WebNavigationFrames.getWindowId(window);
  let parentWindowId = WebNavigationFrames.getParentWindowId(window);
  sendAsyncMessage("Extension:DOMContentLoaded", {windowId, parentWindowId, url});
}

addEventListener("DOMContentLoaded", loadListener);
addMessageListener("Extension:DisableWebNavigation", () => {
  removeEventListener("DOMContentLoaded", loadListener);
});

var WebProgressListener = {
  init: function() {
    // This WeakMap (DOMWindow -> nsIURI) keeps track of the pathname and hash
    // of the previous location for all the existent docShells.
    this.previousURIMap = new WeakMap();

    // Populate the above previousURIMap by iterating over the docShells tree.
    for (let currentDocShell of WebNavigationFrames.iterateDocShellTree(docShell)) {
      let win = currentDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDOMWindow);
      let {currentURI} = currentDocShell.QueryInterface(Ci.nsIWebNavigation);

      this.previousURIMap.set(win, currentURI);
    }

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
      parentWindowId: WebNavigationFrames.getParentWindowId(webProgress.DOMWindow),
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
        this.onLocationChange(webProgress, request, request.QueryInterface(Ci.nsIChannel).URI, 0);
      }
    }
  },

  onLocationChange: function onLocationChange(webProgress, request, locationURI, flags) {
    let {DOMWindow, loadType} = webProgress;

    // Get the previous URI loaded in the DOMWindow.
    let previousURI = this.previousURIMap.get(DOMWindow);

    // Update the URI in the map with the new locationURI.
    this.previousURIMap.set(DOMWindow, locationURI);

    let isSameDocument = (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);
    let isHistoryStateUpdated = false;
    let isReferenceFragmentUpdated = false;

    if (isSameDocument) {
      let pathChanged = !(previousURI && locationURI.equalsExceptRef(previousURI));
      let hashChanged = !(previousURI && previousURI.ref == locationURI.ref);

      // When the location changes but the document is the same:
      // - path not changed and hash changed -> |onReferenceFragmentUpdated|
      //   (even if it changed using |history.pushState|)
      // - path not changed and hash not changed -> |onHistoryStateUpdated|
      //   (only if it changes using |history.pushState|)
      // - path changed -> |onHistoryStateUpdated|

      if (!pathChanged && hashChanged) {
        isReferenceFragmentUpdated = true;
      } else if (loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE) {
        isHistoryStateUpdated = true;
      } else if (loadType & Ci.nsIDocShell.LOAD_CMD_HISTORY) {
        isHistoryStateUpdated = true;
      }
    }

    let data = {
      isHistoryStateUpdated, isReferenceFragmentUpdated,
      location: locationURI ? locationURI.spec : "",
      windowId: webProgress.DOMWindowID,
      parentWindowId: WebNavigationFrames.getParentWindowId(webProgress.DOMWindow),
    };

    sendAsyncMessage("Extension:LocationChange", data);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference]),
};

var disabled = false;
WebProgressListener.init();
addEventListener("unload", () => {
  if (!disabled) {
    disabled = true;
    WebProgressListener.uninit();
  }
});
addMessageListener("Extension:DisableWebNavigation", () => {
  if (!disabled) {
    disabled = true;
    WebProgressListener.uninit();
  }
});
