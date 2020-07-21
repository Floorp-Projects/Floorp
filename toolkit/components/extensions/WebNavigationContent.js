/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env mozilla/frame-script */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "WebNavigationFrames",
  "resource://gre/modules/WebNavigationFrames.jsm"
);

function getDocShellOuterWindowId(docShell) {
  if (!docShell) {
    return undefined;
  }

  return docShell.domWindow.windowUtils.outerWindowID;
}

function loadListener(event) {
  let document = event.target;
  let window = document.defaultView;
  let url = document.documentURI;
  let frameId = WebNavigationFrames.getFrameId(window);
  let parentFrameId = WebNavigationFrames.getParentFrameId(window);
  sendAsyncMessage("Extension:DOMContentLoaded", {
    frameId,
    parentFrameId,
    url,
  });
}

addEventListener("DOMContentLoaded", loadListener);
addMessageListener("Extension:DisableWebNavigation", () => {
  removeEventListener("DOMContentLoaded", loadListener);
});

var CreatedNavigationTargetListener = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  init() {
    Services.obs.addObserver(
      this,
      "webNavigation-createdNavigationTarget-from-js"
    );
  },
  uninit() {
    Services.obs.removeObserver(
      this,
      "webNavigation-createdNavigationTarget-from-js"
    );
  },

  observe(subject, topic, data) {
    if (!(subject instanceof Ci.nsIPropertyBag2)) {
      return;
    }

    let props = subject.QueryInterface(Ci.nsIPropertyBag2);

    const createdDocShell = props.getPropertyAsInterface(
      "createdTabDocShell",
      Ci.nsIDocShell
    );
    const sourceDocShell = props.getPropertyAsInterface(
      "sourceTabDocShell",
      Ci.nsIDocShell
    );

    const isSourceTabDescendant =
      sourceDocShell.sameTypeRootTreeItem === docShell;

    if (
      docShell !== createdDocShell &&
      docShell !== sourceDocShell &&
      !isSourceTabDescendant
    ) {
      // if the createdNavigationTarget is not related to this docShell
      // (this docShell is not the newly created docShell, it is not the source docShell,
      // and the source docShell is not a descendant of it)
      // there is nothing to do here and return early.
      return;
    }

    const isSourceTab = docShell === sourceDocShell || isSourceTabDescendant;

    const sourceFrameId = WebNavigationFrames.getFrameId(
      sourceDocShell.browsingContext
    );
    const createdOuterWindowId = getDocShellOuterWindowId(sourceDocShell);

    let url;
    if (props.hasKey("url")) {
      url = props.getPropertyAsACString("url");
    }

    sendAsyncMessage("Extension:CreatedNavigationTarget", {
      url,
      sourceFrameId,
      createdOuterWindowId,
      isSourceTab,
    });
  },
};

var FormSubmitListener = {
  init() {
    this.formSubmitWindows = new WeakSet();
    addEventListener("DOMFormBeforeSubmit", this);
  },

  uninit() {
    removeEventListener("DOMFormBeforeSubmit", this);
    this.formSubmitWindows = new WeakSet();
  },

  handleEvent({ target: form }) {
    this.formSubmitWindows.add(form.ownerGlobal);
  },

  hasAndForget: function(window) {
    let has = this.formSubmitWindows.has(window);
    this.formSubmitWindows.delete(window);
    return has;
  },
};

var WebProgressListener = {
  init: function() {
    // This WeakMap (DOMWindow -> nsIURI) keeps track of the pathname and hash
    // of the previous location for all the existent docShells.
    this.previousURIMap = new WeakMap();

    // Populate the above previousURIMap by iterating over the docShells tree.
    for (let currentDocShell of WebNavigationFrames.iterateDocShellTree(
      docShell
    )) {
      let win = currentDocShell.domWindow;
      let { currentURI } = currentDocShell.QueryInterface(Ci.nsIWebNavigation);

      this.previousURIMap.set(win, currentURI);
    }

    // This WeakSet of DOMWindows keeps track of the attempted refresh.
    this.refreshAttemptedDOMWindows = new WeakSet();

    let webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_REFRESH |
        Ci.nsIWebProgress.NOTIFY_LOCATION
    );
  },

  uninit() {
    if (!docShell) {
      return;
    }
    let webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);
  },

  onRefreshAttempted: function onRefreshAttempted(
    webProgress,
    URI,
    delay,
    sameURI
  ) {
    this.refreshAttemptedDOMWindows.add(webProgress.DOMWindow);

    // If this function doesn't return true, the attempted refresh will be blocked.
    return true;
  },

  onStateChange: function onStateChange(
    webProgress,
    request,
    stateFlags,
    status
  ) {
    let { originalURI, URI: locationURI } = request.QueryInterface(
      Ci.nsIChannel
    );

    // Prevents "about", "chrome", "resource" and "moz-extension" URI schemes to be
    // reported with the resolved "file" or "jar" URIs. (see Bug 1246125 for rationale)
    if (locationURI.schemeIs("file") || locationURI.schemeIs("jar")) {
      let shouldUseOriginalURI =
        originalURI.schemeIs("about") ||
        originalURI.schemeIs("chrome") ||
        originalURI.schemeIs("resource") ||
        originalURI.schemeIs("moz-extension");

      locationURI = shouldUseOriginalURI ? originalURI : locationURI;
    }

    this.sendStateChange({ webProgress, locationURI, stateFlags, status });

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
    if (
      webProgress.DOMWindow.top != webProgress.DOMWindow &&
      stateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT
    ) {
      this.sendDocumentChange({ webProgress, locationURI, request });
    }
  },

  onLocationChange: function onLocationChange(
    webProgress,
    request,
    locationURI,
    flags
  ) {
    let { DOMWindow } = webProgress;

    // Get the previous URI loaded in the DOMWindow.
    let previousURI = this.previousURIMap.get(DOMWindow);

    // Update the URI in the map with the new locationURI.
    this.previousURIMap.set(DOMWindow, locationURI);

    let isSameDocument =
      flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT;

    // When a frame navigation doesn't change the current loaded document
    // (which can be due to history.pushState/replaceState or to a changed hash in the url),
    // it is reported only to the onLocationChange, for this reason
    // we process the history change here and then we are going to send
    // an "Extension:HistoryChange" to the main process, where it will be turned
    // into a webNavigation.onHistoryStateUpdated/onReferenceFragmentUpdated event.
    if (isSameDocument) {
      this.sendHistoryChange({
        webProgress,
        previousURI,
        locationURI,
        request,
      });
    } else if (webProgress.DOMWindow.top == webProgress.DOMWindow) {
      // We have to catch the document changes from top level frames here,
      // where we can detect the "server redirect" transition.
      // (see Bug 1264936 and Bug 125662 for rationale)
      this.sendDocumentChange({ webProgress, locationURI, request });
    }
  },

  sendStateChange({ webProgress, locationURI, stateFlags, status }) {
    let data = {
      requestURL: locationURI.spec,
      frameId: WebNavigationFrames.getFrameId(webProgress.DOMWindow),
      parentFrameId: WebNavigationFrames.getParentFrameId(
        webProgress.DOMWindow
      ),
      status,
      stateFlags,
    };

    sendAsyncMessage("Extension:StateChange", data);
  },

  sendDocumentChange({ webProgress, locationURI, request }) {
    let { loadType, DOMWindow } = webProgress;
    let frameTransitionData = this.getFrameTransitionData({
      loadType,
      request,
      DOMWindow,
    });

    let data = {
      frameTransitionData,
      location: locationURI ? locationURI.spec : "",
      frameId: WebNavigationFrames.getFrameId(webProgress.DOMWindow),
      parentFrameId: WebNavigationFrames.getParentFrameId(
        webProgress.DOMWindow
      ),
    };

    sendAsyncMessage("Extension:DocumentChange", data);
  },

  sendHistoryChange({ webProgress, previousURI, locationURI, request }) {
    let { loadType, DOMWindow } = webProgress;

    let isHistoryStateUpdated = false;
    let isReferenceFragmentUpdated = false;

    let pathChanged = !(
      previousURI && locationURI.equalsExceptRef(previousURI)
    );
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
      let frameTransitionData = this.getFrameTransitionData({
        loadType,
        request,
        DOMWindow,
      });

      let data = {
        frameTransitionData,
        isHistoryStateUpdated,
        isReferenceFragmentUpdated,
        location: locationURI ? locationURI.spec : "",
        frameId: WebNavigationFrames.getFrameId(webProgress.DOMWindow),
        parentFrameId: WebNavigationFrames.getParentFrameId(
          webProgress.DOMWindow
        ),
      };

      sendAsyncMessage("Extension:HistoryChange", data);
    }
  },

  getFrameTransitionData({ loadType, request, DOMWindow }) {
    let frameTransitionData = {};

    if (loadType & Ci.nsIDocShell.LOAD_CMD_HISTORY) {
      frameTransitionData.forward_back = true;
    }

    if (loadType & Ci.nsIDocShell.LOAD_CMD_RELOAD) {
      frameTransitionData.reload = true;
    }

    if (request instanceof Ci.nsIChannel) {
      if (request.loadInfo.redirectChain.length) {
        frameTransitionData.server_redirect = true;
      }
    }

    if (FormSubmitListener.hasAndForget(DOMWindow)) {
      frameTransitionData.form_submit = true;
    }

    if (this.refreshAttemptedDOMWindows.has(DOMWindow)) {
      this.refreshAttemptedDOMWindows.delete(DOMWindow);
      frameTransitionData.client_redirect = true;
    }

    return frameTransitionData;
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsIWebProgressListener2",
    "nsISupportsWeakReference",
  ]),
};

var disabled = false;
WebProgressListener.init();
FormSubmitListener.init();
CreatedNavigationTargetListener.init();
addEventListener("unload", () => {
  if (!disabled) {
    disabled = true;
    WebProgressListener.uninit();
    FormSubmitListener.uninit();
    CreatedNavigationTargetListener.uninit();
  }
});
addMessageListener("Extension:DisableWebNavigation", () => {
  if (!disabled) {
    disabled = true;
    WebProgressListener.uninit();
    FormSubmitListener.uninit();
    CreatedNavigationTargetListener.uninit();
  }
});
