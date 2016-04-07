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
    let locationURI = request.QueryInterface(Ci.nsIChannel).URI;

    this.sendStateChange({webProgress, locationURI, stateFlags, status});

    // Based on the docs of the webNavigation.onCommitted event, it should be raised when:
    // "The document  might still be downloading, but at least part of
    // the document has been received"
    // and for some reason we don't fire onLocationChange for the
    // initial navigation of a sub-frame.
    // For the above two reasons, when the navigation event is related to
    // a sub-frame we process the document change here and
    // then send an "Extension:DocumentChange" message to the main process,
    // where it will be turned into a webNavigation.onCommitted event.
    // (see Bug 1264936 and Bug 125662 for rationale)
    if ((webProgress.DOMWindow.top != webProgress.DOMWindow) &&
        (stateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT)) {
      this.sendDocumentChange({webProgress, locationURI});
    }
  },

  onLocationChange: function onLocationChange(webProgress, request, locationURI, flags) {
    let {DOMWindow} = webProgress;

    // Get the previous URI loaded in the DOMWindow.
    let previousURI = this.previousURIMap.get(DOMWindow);

    // Update the URI in the map with the new locationURI.
    this.previousURIMap.set(DOMWindow, locationURI);

    let isSameDocument = (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);

    // When a frame navigation doesn't change the current loaded document
    // (which can be due to history.pushState/replaceState or to a changed hash in the url),
    // it is reported only to the onLocationChange, for this reason
    // we process the history change here and then we are going to send
    // an "Extension:HistoryChange" to the main process, where it will be turned
    // into a webNavigation.onHistoryStateUpdated/onReferenceFragmentUpdated event.
    if (isSameDocument) {
      this.sendHistoryChange({webProgress, previousURI, locationURI});
    } else if (webProgress.DOMWindow.top == webProgress.DOMWindow) {
      // We have to catch the document changes from top level frames here,
      // where we can detect the "server redirect" transition.
      // (see Bug 1264936 and Bug 125662 for rationale)
      this.sendDocumentChange({webProgress, locationURI, request});
    }
  },

  sendStateChange({webProgress, locationURI, stateFlags, status}) {
    let data = {
      requestURL: locationURI.spec,
      windowId: webProgress.DOMWindowID,
      parentWindowId: WebNavigationFrames.getParentWindowId(webProgress.DOMWindow),
      status,
      stateFlags,
    };

    sendAsyncMessage("Extension:StateChange", data);
  },

  sendDocumentChange({webProgress, locationURI}) {
    let data = {
      location: locationURI ? locationURI.spec : "",
      windowId: webProgress.DOMWindowID,
      parentWindowId: WebNavigationFrames.getParentWindowId(webProgress.DOMWindow),
    };

    sendAsyncMessage("Extension:DocumentChange", data);
  },

  sendHistoryChange({webProgress, previousURI, locationURI}) {
    let {loadType} = webProgress;

    let isHistoryStateUpdated = false;
    let isReferenceFragmentUpdated = false;

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

    if (isHistoryStateUpdated || isReferenceFragmentUpdated) {
      let data = {
        isHistoryStateUpdated, isReferenceFragmentUpdated,
        location: locationURI ? locationURI.spec : "",
        windowId: webProgress.DOMWindowID,
        parentWindowId: WebNavigationFrames.getParentWindowId(webProgress.DOMWindow),
      };

      sendAsyncMessage("Extension:HistoryChange", data);
    }
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
