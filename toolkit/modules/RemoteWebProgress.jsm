// -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteWebProgress"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function newURI(spec)
{
    return Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService)
                                                    .newURI(spec, null, null);
}

function RemoteWebProgressRequest(spec)
{
  this.uri = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService)
                                                    .newURI(spec, null, null);
}

RemoteWebProgressRequest.prototype = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIChannel]),

  get URI() { return this.uri.clone(); }
};

function RemoteWebProgress(browser)
{
  this._browser = browser;
  this._isLoadingDocument = false;
  this._DOMWindow = null;
  this._isTopLevel = null;
  this._loadType = 0;
  this._progressListeners = [];
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

  _init: function WP_Init() {
    this._browser.messageManager.addMessageListener("Content:StateChange", this);
    this._browser.messageManager.addMessageListener("Content:LocationChange", this);
    this._browser.messageManager.addMessageListener("Content:SecurityChange", this);
    this._browser.messageManager.addMessageListener("Content:StatusChange", this);
  },

  _destroy: function WP_Destroy() {
    this._browser = null;
  },

  get isLoadingDocument() { return this._isLoadingDocument },
  get DOMWindow() { return this._DOMWindow; },
  get DOMWindowID() { return 0; },
  get isTopLevel() {
    // When this object is accessed directly, it's usually obtained
    // through browser.webProgress and thus represents the top-level
    // document.
    // However, during message handling it temporarily represents
    // the webProgress that generated the notification, which may or
    // may not be a toplevel frame.
    return this._isTopLevel === null ? true : this._isTopLevel;
  },
  get loadType() { return this._loadType; },

  addProgressListener: function WP_AddProgressListener (aListener) {
    let listener = aListener.QueryInterface(Ci.nsIWebProgressListener);
    this._progressListeners.push(listener);
  },

  removeProgressListener: function WP_RemoveProgressListener (aListener) {
    this._progressListeners =
      this._progressListeners.filter(function (l) l != aListener);
  },

  _uriSpec: function (spec) {
    if (!spec)
      return null;
    return new RemoteWebProgressRequest(spec);
  },

  receiveMessage: function WP_ReceiveMessage(aMessage) {
    this._isLoadingDocument = aMessage.json.isLoadingDocument;
    this._DOMWindow = aMessage.objects.DOMWindow;
    this._isTopLevel = aMessage.json.isTopLevel;
    this._loadType = aMessage.json.loadType;

    this._browser._contentWindow = aMessage.objects.contentWindow;

    let req = this._uriSpec(aMessage.json.requestURI);
    switch (aMessage.name) {
    case "Content:StateChange":
      for each (let p in this._progressListeners) {
        p.onStateChange(this, req, aMessage.json.stateFlags, aMessage.json.status);
      }
      break;

    case "Content:LocationChange":
      let loc = newURI(aMessage.json.location);

      this._browser.webNavigation._currentURI = loc;
      this._browser.webNavigation.canGoBack = aMessage.json.canGoBack;
      this._browser.webNavigation.canGoForward = aMessage.json.canGoForward;
      this._browser._characterSet = aMessage.json.charset;
      this._browser._documentURI = newURI(aMessage.json.documentURI);
      this._browser._imageDocument = null;

      for each (let p in this._progressListeners) {
        p.onLocationChange(this, req, loc);
      }
      break;

    case "Content:SecurityChange":
      // Invoking this getter triggers the generation of the underlying object,
      // which we need to access with ._securityUI, because .securityUI returns
      // a wrapper that makes _update inaccessible.
      void this._browser.securityUI;
      this._browser._securityUI._update(aMessage.json.state, aMessage.json.status);

      // The state passed might not be correct due to checks performed
      // on the chrome side. _update fixes that.
      for each (let p in this._progressListeners) {
        p.onSecurityChange(this, req, this._browser.securityUI.state);
      }
      break;

    case "Content:StatusChange":
      for each (let p in this._progressListeners) {
        p.onStatusChange(this, req, aMessage.json.status, aMessage.json.message);
      }
      break;
    }

    this._isTopLevel = null;
  }
};
