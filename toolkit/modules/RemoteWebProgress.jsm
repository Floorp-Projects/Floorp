// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteWebProgressManager"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function newURI(spec)
{
    return Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService)
                                                    .newURI(spec, null, null);
}

function RemoteWebProgressRequest(spec, originalSpec)
{
  this._uri = newURI(spec);
  this._originalURI = newURI(originalSpec);
}

RemoteWebProgressRequest.prototype = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIChannel]),

  get URI() { return this._uri.clone(); },
  get originalURI() { return this._originalURI.clone(); }
};

function RemoteWebProgress(aManager, aIsTopLevel) {
  this._manager = aManager;

  this._isLoadingDocument = false;
  this._DOMWindow = null;
  this._isTopLevel = aIsTopLevel;
  this._loadType = 0;
}

RemoteWebProgress.prototype = {
  NOTIFY_STATE_REQUEST:  0x00000001,
  NOTIFY_STATE_DOCUMENT: 0x00000002,
  NOTIFY_STATE_NETWORK:  0x00000004,
  NOTIFY_STATE_WINDOW:   0x00000008,
  NOTIFY_STATE_ALL:      0x0000000f,
  NOTIFY_PROGRESS:       0x00000010,
  NOTIFY_STATUS:         0x00000020,
  NOTIFY_SECURITY:       0x00000040,
  NOTIFY_LOCATION:       0x00000080,
  NOTIFY_REFRESH:        0x00000100,
  NOTIFY_ALL:            0x000001ff,

  get isLoadingDocument() { return this._isLoadingDocument },
  get DOMWindow() { return this._DOMWindow; },
  get DOMWindowID() { return 0; },
  get isTopLevel() { return this._isTopLevel },
  get loadType() { return this._loadType; },

  addProgressListener: function (aListener) {
    this._manager.addProgressListener(aListener);
  },

  removeProgressListener: function (aListener) {
    this._manager.removeProgressListener(aListener);
  }
};

function RemoteWebProgressManager (aBrowser) {
  this._browser = aBrowser;
  this._topLevelWebProgress = new RemoteWebProgress(this, true);
  this._progressListeners = [];

  this._browser.messageManager.addMessageListener("Content:StateChange", this);
  this._browser.messageManager.addMessageListener("Content:LocationChange", this);
  this._browser.messageManager.addMessageListener("Content:SecurityChange", this);
  this._browser.messageManager.addMessageListener("Content:StatusChange", this);
  this._browser.messageManager.addMessageListener("Content:ProgressChange", this);
}

RemoteWebProgressManager.prototype = {
  get topLevelWebProgress() {
    return this._topLevelWebProgress;
  },

  addProgressListener: function (aListener) {
    let listener = aListener.QueryInterface(Ci.nsIWebProgressListener);
    this._progressListeners.push(listener);
  },

  removeProgressListener: function (aListener) {
    this._progressListeners =
      this._progressListeners.filter(l => l != aListener);
  },

  _fixSSLStatusAndState: function (aStatus, aState) {
    let deserialized = null;
    if (aStatus) {
      let helper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Components.interfaces.nsISerializationHelper);

      deserialized = helper.deserializeObject(aStatus)
      deserialized.QueryInterface(Ci.nsISSLStatus);
    }

    return [deserialized, aState];
  },

  setCurrentURI: function (aURI) {
    // This function is simpler than nsDocShell::SetCurrentURI since
    // it doesn't have to deal with child docshells.
    let webNavigation = this._browser.webNavigation;
    webNavigation._currentURI = aURI;

    let webProgress = this.topLevelWebProgress;
    for (let p of this._progressListeners) {
      p.onLocationChange(webProgress, null, aURI);
    }
  },

  _callProgressListeners: function(methodName, ...args) {
    for (let p of this._progressListeners) {
      if (p[methodName]) {
        try {
          p[methodName].apply(p, args);
        } catch (ex) {
          Cu.reportError("RemoteWebProgress failed to call " + methodName + ": " + ex + "\n");
        }
      }
    }
  },

  receiveMessage: function (aMessage) {
    let json = aMessage.json;
    let objects = aMessage.objects;

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
      webProgress._DOMWindow = objects.DOMWindow;
      webProgress._loadType = json.webProgress.loadType;
    }

    // The WebProgressRequest object however is always dynamic.
    let request = null;
    if (json.requestURI) {
      request = new RemoteWebProgressRequest(json.requestURI,
                                             json.originalRequestURI);
    }

    if (isTopLevel) {
      this._browser._contentWindow = objects.contentWindow;
      this._browser._documentContentType = json.documentContentType;
    }

    switch (aMessage.name) {
    case "Content:StateChange":
      this._callProgressListeners("onStateChange", webProgress, request, json.stateFlags, json.status);
      break;

    case "Content:LocationChange":
      let location = newURI(json.location);
      let flags = json.flags;

      // These properties can change even for a sub-frame navigation.
      this._browser.webNavigation.canGoBack = json.canGoBack;
      this._browser.webNavigation.canGoForward = json.canGoForward;

      if (isTopLevel) {
        this._browser.webNavigation._currentURI = location;
        this._browser._characterSet = json.charset;
        this._browser._documentURI = newURI(json.documentURI);
        this._browser._contentTitle = "";
        this._browser._imageDocument = null;
        this._browser._mayEnableCharacterEncodingMenu = json.mayEnableCharacterEncodingMenu;
        this._browser._contentPrincipal = json.principal;
      }

      this._callProgressListeners("onLocationChange", webProgress, request, location, flags);
      break;

    case "Content:SecurityChange":
      let [status, state] = this._fixSSLStatusAndState(json.status, json.state);

      if (isTopLevel) {
        // Invoking this getter triggers the generation of the underlying object,
        // which we need to access with ._securityUI, because .securityUI returns
        // a wrapper that makes _update inaccessible.
        void this._browser.securityUI;
        this._browser._securityUI._update(status, state);
      }

      this._callProgressListeners("onSecurityChange", webProgress, request, state);
      break;

    case "Content:StatusChange":
      this._callProgressListeners("onStatusChange", webProgress, request, json.status, json.message);
      break;

    case "Content:ProgressChange":
      this._callProgressListeners("onProgressChange", webProgress, request, json.curSelf, json.maxSelf, json.curTotal, json.maxTotal);
      break;
    }
  }
};
