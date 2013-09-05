/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/RemoteAddonsChild.jsm");

let WebProgressListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);
  },

  _requestSpec: function (aRequest) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel))
      return null;
    return aRequest.QueryInterface(Ci.nsIChannel).URI.spec;
  },

  _setupJSON: function setupJSON(aWebProgress, aRequest) {
    return {
      isTopLevel: aWebProgress.isTopLevel,
      isLoadingDocument: aWebProgress.isLoadingDocument,
      requestURI: this._requestSpec(aRequest),
      loadType: aWebProgress.loadType
    };
  },

  _setupObjects: function setupObjects(aWebProgress) {
    let win = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindow);
    return {
      contentWindow: win,
      // DOMWindow is not necessarily the content-window with subframes.
      DOMWindow: aWebProgress.DOMWindow
    };
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.stateFlags = aStateFlags;
    json.status = aStatus;

    sendAsyncMessage("Content:StateChange", json, objects);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.location = aLocationURI ? aLocationURI.spec : "";

    if (json.isTopLevel) {
      json.canGoBack = docShell.canGoBack;
      json.canGoForward = docShell.canGoForward;
      json.documentURI = content.document.documentURIObject.spec;
      json.charset = content.document.characterSet;
    }

    sendAsyncMessage("Content:LocationChange", json, objects);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.status = aStatus;
    json.message = aMessage;

    sendAsyncMessage("Content:StatusChange", json, objects);
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.state = aState;
    json.status = SecurityUI.getSSLStatusAsString();

    sendAsyncMessage("Content:SecurityChange", json, objects);
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

WebProgressListener.init();

let WebNavigation =  {
  _webNavigation: docShell.QueryInterface(Ci.nsIWebNavigation),

  init: function() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "WebNavigation:GoBack":
        this.goBack();
        break;
      case "WebNavigation:GoForward":
        this.goForward();
        break;
      case "WebNavigation:GotoIndex":
        this.gotoIndex(message);
        break;
      case "WebNavigation:LoadURI":
        this.loadURI(message);
        break;
      case "WebNavigation:Reload":
        this.reload(message);
        break;
      case "WebNavigation:Stop":
        this.stop(message);
        break;
    }
  },

  goBack: function() {
    if (this._webNavigation.canGoBack)
      this._webNavigation.goBack();
  },

  goForward: function() {
    if (this._webNavigation.canGoForward)
      this._webNavigation.goForward();
  },

  gotoIndex: function(message) {
    this._webNavigation.gotoIndex(message.index);
  },

  loadURI: function(message) {
    let flags = message.json.flags || this._webNavigation.LOAD_FLAGS_NONE;
    this._webNavigation.loadURI(message.json.uri, flags, null, null, null);
  },

  reload: function(message) {
    let flags = message.json.flags || this._webNavigation.LOAD_FLAGS_NONE;
    this._webNavigation.reload(flags);
  },

  stop: function(message) {
    let flags = message.json.flags || this._webNavigation.STOP_ALL;
    this._webNavigation.stop(flags);
  }
};

WebNavigation.init();

let SecurityUI = {
  getSSLStatusAsString: function() {
    let status = docShell.securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    if (status) {
      let helper = Cc["@mozilla.org/network/serialization-helper;1"]
                      .getService(Ci.nsISerializationHelper);

      status.QueryInterface(Ci.nsISerializable);
      return helper.serializeToString(status);
    }

    return null;
  }
};

let ControllerCommands = {
  init: function () {
    addMessageListener("ControllerCommands:Do", this);
  },

  receiveMessage: function(message) {
    switch(message.name) {
      case "ControllerCommands:Do":
        if (docShell.isCommandEnabled(message.data))
          docShell.doCommand(message.data);
        break;
    }
  }
}

ControllerCommands.init()

addEventListener("DOMTitleChanged", function (aEvent) {
  let document = content.document;
  switch (aEvent.type) {
  case "DOMTitleChanged":
    if (!aEvent.isTrusted || aEvent.target.defaultView != content)
      return;

    sendAsyncMessage("DOMTitleChanged", { title: document.title });
    break;
  }
}, false);

addEventListener("ImageContentLoaded", function (aEvent) {
  if (content.document instanceof Ci.nsIImageDocument) {
    let req = content.document.imageRequest;
    if (!req.image)
      return;
    sendAsyncMessage("ImageDocumentLoaded", { width: req.image.width,
                                              height: req.image.height });
  }
}, false);

RemoteAddonsChild.init(this);
