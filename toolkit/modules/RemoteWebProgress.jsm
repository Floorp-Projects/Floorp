// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteWebProgressManager"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

function RemoteWebProgressRequest(uriOrSpec, originalURIOrSpec, matchedList) {
  this.wrappedJSObject = this;

  this._uri = uriOrSpec instanceof Ci.nsIURI ? uriOrSpec
                                          : Services.io.newURI(uriOrSpec);
  this._originalURI = originalURIOrSpec instanceof Ci.nsIURI ?
                        originalURIOrSpec : Services.io.newURI(originalURIOrSpec);
  this._matchedList = matchedList;
}

RemoteWebProgressRequest.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIChannel, Ci.nsIClassifiedChannel]),

  get URI() { return this._uri; },
  get originalURI() { return this._originalURI; },
  get matchedList() { return this._matchedList; },
};

function RemoteWebProgress(aManager, aIsTopLevel) {
  this.wrappedJSObject = this;

  this._manager = aManager;

  this._isLoadingDocument = false;
  this._DOMWindowID = 0;
  this._isTopLevel = aIsTopLevel;
  this._loadType = 0;
}

RemoteWebProgress.prototype = {
  NOTIFY_STATE_REQUEST:    0x00000001,
  NOTIFY_STATE_DOCUMENT:   0x00000002,
  NOTIFY_STATE_NETWORK:    0x00000004,
  NOTIFY_STATE_WINDOW:     0x00000008,
  NOTIFY_STATE_ALL:        0x0000000f,
  NOTIFY_PROGRESS:         0x00000010,
  NOTIFY_STATUS:           0x00000020,
  NOTIFY_SECURITY:         0x00000040,
  NOTIFY_LOCATION:         0x00000080,
  NOTIFY_REFRESH:          0x00000100,
  NOTIFY_CONTENT_BLOCKING: 0x00000200,
  NOTIFY_ALL:              0x000003ff,

  get isLoadingDocument() { return this._isLoadingDocument; },
  get DOMWindow() {
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },
  get DOMWindowID() { return this._DOMWindowID; },
  get isTopLevel() { return this._isTopLevel; },
  get loadType() { return this._loadType; },

  addProgressListener(aListener, aNotifyMask) {
    this._manager.addProgressListener(aListener, aNotifyMask);
  },

  removeProgressListener(aListener) {
    this._manager.removeProgressListener(aListener);
  },
};

function RemoteWebProgressManager(aBrowser) {
  this._topLevelWebProgress = new RemoteWebProgress(this, true);
  this._progressListeners = [];

  this.swapBrowser(aBrowser);
}

RemoteWebProgressManager.prototype = {
  swapBrowser(aBrowser) {
    if (this._messageManager) {
      this._messageManager.removeMessageListener("Content:StateChange", this);
      this._messageManager.removeMessageListener("Content:LocationChange", this);
      this._messageManager.removeMessageListener("Content:SecurityChange", this);
      this._messageManager.removeMessageListener("Content:StatusChange", this);
      this._messageManager.removeMessageListener("Content:ProgressChange", this);
      this._messageManager.removeMessageListener("Content:LoadURIResult", this);
    }

    this._browser = aBrowser;
    this._messageManager = aBrowser.messageManager;
    this._messageManager.addMessageListener("Content:StateChange", this);
    this._messageManager.addMessageListener("Content:LocationChange", this);
    this._messageManager.addMessageListener("Content:SecurityChange", this);
    this._messageManager.addMessageListener("Content:StatusChange", this);
    this._messageManager.addMessageListener("Content:ProgressChange", this);
    this._messageManager.addMessageListener("Content:LoadURIResult", this);
  },

  swapListeners(aOtherRemoteWebProgressManager) {
    let temp = aOtherRemoteWebProgressManager.progressListeners;
    aOtherRemoteWebProgressManager._progressListeners = this._progressListeners;
    this._progressListeners = temp;
  },

  get progressListeners() {
    return this._progressListeners;
  },

  get topLevelWebProgress() {
    return this._topLevelWebProgress;
  },

  addProgressListener(aListener, aNotifyMask) {
    let listener = aListener.QueryInterface(Ci.nsIWebProgressListener);
    this._progressListeners.push({
      listener,
      mask: aNotifyMask || Ci.nsIWebProgress.NOTIFY_ALL,
    });
  },

  removeProgressListener(aListener) {
    this._progressListeners =
      this._progressListeners.filter(l => l.listener != aListener);
  },

  _fixSecInfoAndState(aSecInfo, aState) {
    let deserialized = null;
    if (aSecInfo) {
      let helper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);

      deserialized = helper.deserializeObject(aSecInfo);
      deserialized.QueryInterface(Ci.nsITransportSecurityInfo);
    }

    return [deserialized, aState];
  },

  setCurrentURI(aURI) {
    // This function is simpler than nsDocShell::SetCurrentURI since
    // it doesn't have to deal with child docshells.
    let remoteWebNav = this._browser._remoteWebNavigationImpl;
    remoteWebNav._currentURI = aURI;

    let webProgress = this.topLevelWebProgress;
    for (let { listener, mask } of this._progressListeners) {
      if (mask & Ci.nsIWebProgress.NOTIFY_LOCATION) {
        listener.onLocationChange(webProgress, null, aURI);
      }
    }
  },

  _callProgressListeners(type, methodName, ...args) {
    for (let { listener, mask } of this._progressListeners) {
      if ((mask & type) && listener[methodName]) {
        try {
          listener[methodName].apply(listener, args);
        } catch (ex) {
          Cu.reportError("RemoteWebProgress failed to call " + methodName + ": " + ex + "\n");
        }
      }
    }
  },

  callWebProgressContentBlockingEventListeners(aIsWebProgressPassed,
                                               aIsTopLevel,
                                               aIsLoadingDocument,
                                               aLoadType,
                                               aDOMWindowID,
                                               aRequestURI,
                                               aOriginalRequestURI,
                                               aMatchedList,
                                               aEvent) {
    let [webProgress, request] =
      this.getWebProgressAndRequest(aIsWebProgressPassed, aIsTopLevel,
                                    aIsLoadingDocument, aLoadType,
                                    aDOMWindowID, aRequestURI,
                                    aOriginalRequestURI, aMatchedList);
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING, "onContentBlockingEvent",
      webProgress, request, aEvent
    );
  },

  getWebProgressAndRequest(aIsWebProgressPassed,
                           aIsTopLevel,
                           aIsLoadingDocument,
                           aLoadType,
                           aDOMWindowID,
                           aRequestURI,
                           aOriginalRequestURI,
                           aMatchedList) {
    let webProgress = null;
    if (aIsWebProgressPassed) {
      // The top-level WebProgress is always the same, but because we don't
      // really have a concept of subframes/content we always create a new object
      // for those.
      webProgress = aIsTopLevel ? this._topLevelWebProgress
                                : new RemoteWebProgress(this, false);

      // Update the actual WebProgress fields.
      webProgress._isLoadingDocument = aIsLoadingDocument;
      webProgress._DOMWindowID = aDOMWindowID;
      webProgress._loadType = aLoadType;
    }

    let request =
      aRequestURI ?
        new RemoteWebProgressRequest(aRequestURI,
                                     aOriginalRequestURI,
                                     aMatchedList) :
        null;

    return [webProgress, request];
  },

  receiveMessage(aMessage) {
    let json = aMessage.json;
    // This message is a custom one we send as a result of a loadURI call.
    // It shouldn't go through the same processing as all the forwarded
    // webprogresslistener messages.
    if (aMessage.name == "Content:LoadURIResult") {
      this._browser.inLoadURI = false;
      return;
    }

    let webProgress = null;
    let isTopLevel = json.webProgress && json.webProgress.isTopLevel;
    // The top-level WebProgress is always the same, but because we don't
    // really have a concept of subframes/content we always create a new object
    // for those.
    if (json.webProgress) {
      webProgress = isTopLevel ? this._topLevelWebProgress
                               : new RemoteWebProgress(this, false);

      // Update the actual WebProgress fields.
      webProgress._isLoadingDocument = json.webProgress.isLoadingDocument;
      webProgress._DOMWindowID = json.webProgress.DOMWindowID;
      webProgress._loadType = json.webProgress.loadType;
    }

    // The WebProgressRequest object however is always dynamic.
    let request = null;
    if (json.requestURI) {
      request = new RemoteWebProgressRequest(json.requestURI,
                                             json.originalRequestURI,
                                             json.matchedList);
    }

    if (isTopLevel) {
      // Setting a content-type back to `null` is quite nonsensical for the
      // frontend, especially since we're not expecting it.
      if (json.documentContentType !== null) {
        this._browser._documentContentType = json.documentContentType;
      }
      if (typeof json.inLoadURI != "undefined") {
        this._browser.inLoadURI = json.inLoadURI;
      }
      if (json.charset) {
        this._browser._characterSet = json.charset;
        this._browser._mayEnableCharacterEncodingMenu = json.mayEnableCharacterEncodingMenu;
      }
    }

    switch (aMessage.name) {
    case "Content:StateChange":
      if (isTopLevel) {
        this._browser._documentURI = Services.io.newURI(json.documentURI);
      }
      this._callProgressListeners(
        Ci.nsIWebProgress.NOTIFY_STATE_ALL, "onStateChange", webProgress,
        request, json.stateFlags, json.status
      );
      break;

    case "Content:LocationChange":
      let location = Services.io.newURI(json.location);
      let flags = json.flags;
      let remoteWebNav = this._browser._remoteWebNavigationImpl;

      // These properties can change even for a sub-frame navigation.
      remoteWebNav.canGoBack = json.canGoBack;
      remoteWebNav.canGoForward = json.canGoForward;

      if (isTopLevel) {
        remoteWebNav._currentURI = location;
        this._browser._documentURI = Services.io.newURI(json.documentURI);
        this._browser._contentTitle = json.title;
        this._browser._imageDocument = null;
        this._browser._contentPrincipal = json.principal;
        this._browser._isSyntheticDocument = json.synthetic;
        this._browser._innerWindowID = json.innerWindowID;
        this._browser._contentRequestContextID = json.requestContextID;
      }

      this._callProgressListeners(
        Ci.nsIWebProgress.NOTIFY_LOCATION, "onLocationChange", webProgress,
        request, location, flags
      );
      break;

    case "Content:SecurityChange":
      let [secInfo, state] = this._fixSecInfoAndState(json.secInfo, json.state);

      if (isTopLevel) {
        // Invoking this getter triggers the generation of the underlying object,
        // which we need to access with ._securityUI, because .securityUI returns
        // a wrapper that makes _update inaccessible.
        void this._browser.securityUI;
        this._browser._securityUI._update(secInfo, state);
      }

      this._callProgressListeners(
        Ci.nsIWebProgress.NOTIFY_SECURITY, "onSecurityChange", webProgress,
        request, state
      );
      break;

    case "Content:StatusChange":
      this._callProgressListeners(
        Ci.nsIWebProgress.NOTIFY_STATUS, "onStatusChange", webProgress, request,
        json.status, json.message
      );
      break;

    case "Content:ProgressChange":
      this._callProgressListeners(
        Ci.nsIWebProgress.NOTIFY_PROGRESS, "onProgressChange", webProgress,
        request, json.curSelf, json.maxSelf, json.curTotal, json.maxTotal
      );
      break;
    }
  },
};
