/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/RemoteAddonsChild.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm");

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
#endif

let FocusSyncHandler = {
  init: function() {
    sendAsyncMessage("SetSyncHandler", {}, {handler: this});
  },

  getFocusedElementAndWindow: function() {
    let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

    let focusedWindow = {};
    let elt = fm.getFocusedElementForWindow(content, true, focusedWindow);
    return [elt, focusedWindow.value];
  },
};

FocusSyncHandler.init();

let WebProgressListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);
  },

  _requestSpec: function (aRequest, aPropertyName) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel))
      return null;
    return aRequest.QueryInterface(Ci.nsIChannel)[aPropertyName].spec;
  },

  _setupJSON: function setupJSON(aWebProgress, aRequest) {
    return {
      isTopLevel: aWebProgress.isTopLevel,
      isLoadingDocument: aWebProgress.isLoadingDocument,
      requestURI: this._requestSpec(aRequest, "URI"),
      originalRequestURI: this._requestSpec(aRequest, "originalURI"),
      loadType: aWebProgress.loadType,
      documentContentType: content.document && content.document.contentType
    };
  },

  _setupObjects: function setupObjects(aWebProgress) {
    return {
      contentWindow: content,
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
    json.flags = aFlags;

    // These properties can change even for a sub-frame navigation.
    json.canGoBack = docShell.canGoBack;
    json.canGoForward = docShell.canGoForward;

    if (json.isTopLevel) {
      json.documentURI = content.document.documentURIObject.spec;
      json.charset = content.document.characterSet;
      json.mayEnableCharacterEncodingMenu = docShell.mayEnableCharacterEncodingMenu;
      json.principal = content.document.nodePrincipal;
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
  init: function() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);

    this._webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    this._sessionHistory = this._webNavigation.sessionHistory;

    // Send a CPOW for the sessionHistory object. We need to make sure
    // it stays alive as long as the content script since CPOWs are
    // weakly held.
    let history = this._sessionHistory;
    sendAsyncMessage("WebNavigation:setHistory", {}, {history: history});
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
        this.gotoIndex(message.data.index);
        break;
      case "WebNavigation:LoadURI":
        this.loadURI(message.data.uri, message.data.flags, message.data.referrer);
        break;
      case "WebNavigation:Reload":
        this.reload(message.data.flags);
        break;
      case "WebNavigation:Stop":
        this.stop(message.data.flags);
        break;
    }
  },

  goBack: function() {
    if (this._webNavigation.canGoBack) {
      this._webNavigation.goBack();
    }
  },

  goForward: function() {
    if (this._webNavigation.canGoForward)
      this._webNavigation.goForward();
  },

  gotoIndex: function(index) {
    this._webNavigation.gotoIndex(index);
  },

  loadURI: function(uri, flags, referrer) {
#ifdef MOZ_CRASHREPORTER
    if (CrashReporter.enabled)
      CrashReporter.annotateCrashReport("URL", uri);
#endif
    if (referrer)
      referrer = Services.io.newURI(referrer, null, null);
    this._webNavigation.loadURI(uri, flags, referrer, null, null);
  },

  reload: function(flags) {
    this._webNavigation.reload(flags);
  },

  stop: function(flags) {
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

addEventListener("DOMWindowClose", function (aEvent) {
  if (!aEvent.isTrusted)
    return;
  sendAsyncMessage("DOMWindowClose");
  aEvent.preventDefault();
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

let DocumentObserver = {
  init: function() {
    Services.obs.addObserver(this, "document-element-inserted", false);
    addEventListener("unload", () => {
      Services.obs.removeObserver(this, "document-element-inserted");
    });
  },

  observe: function(aSubject, aTopic, aData) {
    if (aSubject == content.document) {
      sendAsyncMessage("DocumentInserted", {synthetic: aSubject.mozSyntheticDocument});
    }
  },
};
DocumentObserver.init();

const ZoomManager = {
  get fullZoom() {
    return this._cache.fullZoom;
  },

  get textZoom() {
    return this._cache.textZoom;
  },

  set fullZoom(value) {
    this._cache.fullZoom = value;
    this._markupViewer.fullZoom = value;
  },

  set textZoom(value) {
    this._cache.textZoom = value;
    this._markupViewer.textZoom = value;
  },

  refreshFullZoom: function() {
    return this._refreshZoomValue('fullZoom');
  },

  refreshTextZoom: function() {
    return this._refreshZoomValue('textZoom');
  },

  /**
   * Retrieves specified zoom property value from markupViewer and refreshes
   * cache if needed.
   * @param valueName Either 'fullZoom' or 'textZoom'.
   * @returns Returns true if cached value was actually refreshed.
   * @private
   */
  _refreshZoomValue: function(valueName) {
    let actualZoomValue = this._markupViewer[valueName];
    if (actualZoomValue != this._cache[valueName]) {
      this._cache[valueName] = actualZoomValue;
      return true;
    }
    return false;
  },

  get _markupViewer() {
    return docShell.contentViewer;
  },

  _cache: {
    fullZoom: NaN,
    textZoom: NaN
  }
};

addMessageListener("FullZoom", function (aMessage) {
  ZoomManager.fullZoom = aMessage.data.value;
});

addMessageListener("TextZoom", function (aMessage) {
  ZoomManager.textZoom = aMessage.data.value;
});

addEventListener("FullZoomChange", function () {
  if (ZoomManager.refreshFullZoom()) {
    sendAsyncMessage("FullZoomChange", { value:  ZoomManager.fullZoom});
  }
}, false);

addEventListener("TextZoomChange", function (aEvent) {
  if (ZoomManager.refreshTextZoom()) {
    sendAsyncMessage("TextZoomChange", { value:  ZoomManager.textZoom});
  }
}, false);

addEventListener("ZoomChangeUsingMouseWheel", function () {
  sendAsyncMessage("ZoomChangeUsingMouseWheel", {});
}, false);

addMessageListener("UpdateCharacterSet", function (aMessage) {
  docShell.charset = aMessage.data.value;
  docShell.gatherCharsetMenuTelemetry();
});

/**
 * Remote thumbnail request handler for PageThumbs thumbnails.
 */
addMessageListener("Browser:Thumbnail:Request", function (aMessage) {
  let thumbnail = content.document.createElementNS(PageThumbUtils.HTML_NAMESPACE,
                                                   "canvas");
  thumbnail.mozOpaque = true;
  thumbnail.mozImageSmoothingEnabled = true;

  thumbnail.width = aMessage.data.canvasWidth;
  thumbnail.height = aMessage.data.canvasHeight;

  let [width, height, scale] =
    PageThumbUtils.determineCropSize(content, thumbnail);

  let ctx = thumbnail.getContext("2d");
  ctx.save();
  ctx.scale(scale, scale);
  ctx.drawWindow(content, 0, 0, width, height,
                 aMessage.data.background,
                 ctx.DRAWWINDOW_DO_NOT_FLUSH);
  ctx.restore();

  thumbnail.toBlob(function (aBlob) {
    sendAsyncMessage("Browser:Thumbnail:Response", {
      thumbnail: aBlob,
      id: aMessage.data.id
    });
  });
});

// The AddonsChild needs to be rooted so that it stays alive as long as
// the tab.
let AddonsChild;
if (Services.appinfo.browserTabsRemoteAutostart) {
  // Currently, the addon shims are only supported when autostarting
  // with remote tabs.
  AddonsChild = RemoteAddonsChild.init(this);

  addEventListener("unload", () => {
    RemoteAddonsChild.uninit(AddonsChild);
  });
}

addMessageListener("NetworkPrioritizer:AdjustPriority", (msg) => {
  let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
  let loadGroup = webNav.QueryInterface(Ci.nsIDocumentLoader)
                        .loadGroup.QueryInterface(Ci.nsISupportsPriority);
  loadGroup.adjustPriority(msg.data.adjustment);
});

let DOMFullscreenManager = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  init: function() {
    Services.obs.addObserver(this, "ask-parent-to-exit-fullscreen", false);
    Services.obs.addObserver(this, "ask-parent-to-rollback-fullscreen", false);
    addMessageListener("DOMFullscreen:ChildrenMustExit", () => {
      let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
      utils.exitFullscreen();
    });
    addEventListener("unload", () => {
      Services.obs.removeObserver(this, "ask-parent-to-exit-fullscreen");
      Services.obs.removeObserver(this, "ask-parent-to-rollback-fullscreen");
    });
  },

  observe: function(aSubject, aTopic, aData) {
    // Observer notifications are global, which means that these notifications
    // might be coming from elements that are not actually children within this
    // windows' content. We should ignore those. This will not be necessary once
    // we fix bug 1053413 and stop using observer notifications for this stuff.
    if (aSubject.defaultView.top !== content) {
      return;
    }

    switch (aTopic) {
      case "ask-parent-to-exit-fullscreen": {
        sendAsyncMessage("DOMFullscreen:RequestExit");
        break;
      }
      case "ask-parent-to-rollback-fullscreen": {
        sendAsyncMessage("DOMFullscreen:RequestRollback");
        break;
      }
    }
  },
};

DOMFullscreenManager.init();

let AutoCompletePopup = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup]),

  init: function() {
    // Hook up the form fill autocomplete controller.
    let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                       .getService(Ci.nsIFormFillController);

    controller.attachToBrowser(docShell, this.QueryInterface(Ci.nsIAutoCompletePopup));

    this._input = null;
    this._popupOpen = false;

    addMessageListener("FormAutoComplete:HandleEnter", message => {
      this.selectedIndex = message.data.selectedIndex;

      let controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                  getService(Components.interfaces.nsIAutoCompleteController);
      controller.handleEnter(message.data.isPopupSelection);
    });
  },

  get input () { return this._input; },
  get overrideValue () { return null; },
  set selectedIndex (index) { },
  get selectedIndex () {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    return sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
  },
  get popupOpen () {
    return this._popupOpen;
  },

  openAutocompletePopup: function (input, element) {
    this._input = input;
    this._popupOpen = true;
  },

  closePopup: function () {
    this._popupOpen = false;
    sendAsyncMessage("FormAutoComplete:ClosePopup", {});
  },

  invalidate: function () {
  },

  selectBy: function(reverse, page) {
    this._index = sendSyncMessage("FormAutoComplete:SelectBy", {
      reverse: reverse,
      page: page
    });
  }
}

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
let initData = sendSyncMessage("Browser:Init");
if (initData.length) {
  docShell.useGlobalHistory = initData[0].useGlobalHistory;
  if (initData[0].initPopup) {
    setTimeout(() => AutoCompletePopup.init(), 0);
  }
}
