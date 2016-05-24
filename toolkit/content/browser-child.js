/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/RemoteAddonsChild.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm");

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
                                     "@mozilla.org/xre/app-info;1",
                                     "nsICrashReporter");
}

function makeInputStream(aString) {
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(Ci.nsISupportsCString);
  stream.data = aString;
  return stream; // XPConnect will QI this to nsIInputStream for us.
}

var WebProgressListener = {
  init: function() {
    this._filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);

    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, Ci.nsIWebProgress.NOTIFY_ALL);
  },

  uninit() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this._filter);

    this._filter.removeProgressListener(this);
    this._filter = null;
  },

  _requestSpec: function (aRequest, aPropertyName) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel))
      return null;
    return aRequest.QueryInterface(Ci.nsIChannel)[aPropertyName].spec;
  },

  _setupJSON: function setupJSON(aWebProgress, aRequest) {
    let innerWindowID = null;
    if (aWebProgress) {
      let domWindowID = null;
      try {
        let utils = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);
        domWindowID = utils.outerWindowID;
        innerWindowID = utils.currentInnerWindowID;
      } catch (e) {
        // If nsDocShell::Destroy has already been called, then we'll
        // get NS_NOINTERFACE when trying to get the DOM window.
        // If there is no current inner window, we'll get
        // NS_ERROR_NOT_AVAILABLE.
      }

      aWebProgress = {
        isTopLevel: aWebProgress.isTopLevel,
        isLoadingDocument: aWebProgress.isLoadingDocument,
        loadType: aWebProgress.loadType,
        DOMWindowID: domWindowID
      };
    }

    return {
      webProgress: aWebProgress || null,
      requestURI: this._requestSpec(aRequest, "URI"),
      originalRequestURI: this._requestSpec(aRequest, "originalURI"),
      documentContentType: content.document && content.document.contentType,
      innerWindowID,
    };
  },

  _setupObjects: function setupObjects(aWebProgress, aRequest) {
    let domWindow;
    try {
      domWindow = aWebProgress && aWebProgress.DOMWindow;
    } catch (e) {
      // If nsDocShell::Destroy has already been called, then we'll
      // get NS_NOINTERFACE when trying to get the DOM window. Ignore
      // that here.
      domWindow = null;
    }

    return {
      contentWindow: content,
      // DOMWindow is not necessarily the content-window with subframes.
      DOMWindow: domWindow,
      webProgress: aWebProgress,
      request: aRequest,
    };
  },

  _send(name, data, objects) {
    if (RemoteAddonsChild.useSyncWebProgress) {
      sendRpcMessage(name, data, objects);
    } else {
      sendAsyncMessage(name, data, objects);
    }
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress, aRequest);

    json.stateFlags = aStateFlags;
    json.status = aStatus;

    // It's possible that this state change was triggered by
    // loading an internal error page, for which the parent
    // will want to know some details, so we'll update it with
    // the documentURI.
    if (aWebProgress && aWebProgress.isTopLevel) {
      json.documentURI = content.document.documentURIObject.spec;
      json.charset = content.document.characterSet;
      json.mayEnableCharacterEncodingMenu = docShell.mayEnableCharacterEncodingMenu;
      json.inLoadURI = WebNavigation.inLoadURI;
    }

    this._send("Content:StateChange", json, objects);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress, aRequest);

    json.curSelf = aCurSelf;
    json.maxSelf = aMaxSelf;
    json.curTotal = aCurTotal;
    json.maxTotal = aMaxTotal;

    this._send("Content:ProgressChange", json, objects);
  },

  onProgressChange64: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    this.onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal);
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress, aRequest);

    json.location = aLocationURI ? aLocationURI.spec : "";
    json.flags = aFlags;

    // These properties can change even for a sub-frame navigation.
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    json.canGoBack = webNav.canGoBack;
    json.canGoForward = webNav.canGoForward;

    if (aWebProgress && aWebProgress.isTopLevel) {
      json.documentURI = content.document.documentURIObject.spec;
      json.title = content.document.title;
      json.charset = content.document.characterSet;
      json.mayEnableCharacterEncodingMenu = docShell.mayEnableCharacterEncodingMenu;
      json.principal = content.document.nodePrincipal;
      json.synthetic = content.document.mozSyntheticDocument;
      json.inLoadURI = WebNavigation.inLoadURI;

      if (AppConstants.MOZ_CRASHREPORTER && CrashReporter.enabled) {
        let uri = aLocationURI.clone();
        try {
          // If the current URI contains a username/password, remove it.
          uri.userPass = "";
        } catch (ex) { /* Ignore failures on about: URIs. */ }
        CrashReporter.annotateCrashReport("URL", uri.spec);
      }
    }

    this._send("Content:LocationChange", json, objects);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress, aRequest);

    json.status = aStatus;
    json.message = aMessage;

    this._send("Content:StatusChange", json, objects);
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress, aRequest);

    json.state = aState;
    json.status = SecurityUI.getSSLStatusAsString();

    this._send("Content:SecurityChange", json, objects);
  },

  onRefreshAttempted: function onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
    return true;
  },

  sendLoadCallResult() {
    sendAsyncMessage("Content:LoadURIResult");
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

WebProgressListener.init();
addEventListener("unload", () => {
  WebProgressListener.uninit();
});

var WebNavigation =  {
  init: function() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);
  },

  get webNavigation() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
  },

  _inLoadURI: false,

  get inLoadURI() {
    return this._inLoadURI;
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
        this.loadURI(message.data.uri, message.data.flags,
                     message.data.referrer, message.data.referrerPolicy,
                     message.data.postData, message.data.headers,
                     message.data.baseURI);
        break;
      case "WebNavigation:Reload":
        this.reload(message.data.flags);
        break;
      case "WebNavigation:Stop":
        this.stop(message.data.flags);
        break;
    }
  },

  _wrapURIChangeCall(fn) {
    this._inLoadURI = true;
    try {
      fn();
    } finally {
      this._inLoadURI = false;
      WebProgressListener.sendLoadCallResult();
    }
  },

  goBack: function() {
    if (this.webNavigation.canGoBack) {
      this._wrapURIChangeCall(() => this.webNavigation.goBack());
    }
  },

  goForward: function() {
    if (this.webNavigation.canGoForward) {
      this._wrapURIChangeCall(() => this.webNavigation.goForward());
    }
  },

  gotoIndex: function(index) {
    this._wrapURIChangeCall(() => this.webNavigation.gotoIndex(index));
  },

  loadURI: function(uri, flags, referrer, referrerPolicy, postData, headers, baseURI) {
    if (AppConstants.MOZ_CRASHREPORTER && CrashReporter.enabled) {
      let annotation = uri;
      try {
        let url = Services.io.newURI(uri, null, null);
        // If the current URI contains a username/password, remove it.
        url.userPass = "";
        annotation = url.spec;
      } catch (ex) { /* Ignore failures to parse and failures
                      on about: URIs. */ }
      CrashReporter.annotateCrashReport("URL", annotation);
    }
    if (referrer)
      referrer = Services.io.newURI(referrer, null, null);
    if (postData)
      postData = makeInputStream(postData);
    if (headers)
      headers = makeInputStream(headers);
    if (baseURI)
      baseURI = Services.io.newURI(baseURI, null, null);
    this._wrapURIChangeCall(() => {
      return this.webNavigation.loadURIWithOptions(uri, flags, referrer, referrerPolicy,
                                                   postData, headers, baseURI);
    });
  },

  reload: function(flags) {
    this.webNavigation.reload(flags);
  },

  stop: function(flags) {
    this.webNavigation.stop(flags);
  }
};

WebNavigation.init();

var SecurityUI = {
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

var ControllerCommands = {
  init: function () {
    addMessageListener("ControllerCommands:Do", this);
    addMessageListener("ControllerCommands:DoWithParams", this);
  },

  receiveMessage: function(message) {
    switch(message.name) {
      case "ControllerCommands:Do":
        if (docShell.isCommandEnabled(message.data))
          docShell.doCommand(message.data);
        break;

      case "ControllerCommands:DoWithParams":
        var data = message.data;
        if (docShell.isCommandEnabled(data.cmd)) {
          var params = Cc["@mozilla.org/embedcomp/command-params;1"].
                       createInstance(Ci.nsICommandParams);
          for (var name in data.params) {
            var value = data.params[name];
            if (value.type == "long") {
              params.setLongValue(name, parseInt(value.value));
            } else {
              throw Cr.NS_ERROR_NOT_IMPLEMENTED;
            }
          }
          docShell.doCommandWithParams(data.cmd, params);
        }
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
    // Round to remove any floating-point error.
    actualZoomValue = Number(actualZoomValue.toFixed(2));
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
    sendAsyncMessage("FullZoomChange", { value: ZoomManager.fullZoom });
  }
}, false);

addEventListener("TextZoomChange", function (aEvent) {
  if (ZoomManager.refreshTextZoom()) {
    sendAsyncMessage("TextZoomChange", { value: ZoomManager.textZoom });
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
  let snapshot;
  let args = aMessage.data.additionalArgs;
  let fullScale = args ? args.fullScale : false;
  if (fullScale) {
    snapshot = PageThumbUtils.createSnapshotThumbnail(content, null, args);
  } else {
    let snapshotWidth = aMessage.data.canvasWidth;
    let snapshotHeight = aMessage.data.canvasHeight;
    snapshot =
      PageThumbUtils.createCanvas(content, snapshotWidth, snapshotHeight);
    PageThumbUtils.createSnapshotThumbnail(content, snapshot, args);
  }

  snapshot.toBlob(function (aBlob) {
    sendAsyncMessage("Browser:Thumbnail:Response", {
      thumbnail: aBlob,
      id: aMessage.data.id
    });
  });
});

/**
 * Remote isSafeForCapture request handler for PageThumbs.
 */
addMessageListener("Browser:Thumbnail:CheckState", function (aMessage) {
  let result = PageThumbUtils.shouldStoreContentThumbnail(content, docShell);
  sendAsyncMessage("Browser:Thumbnail:CheckState:Response", {
    result: result
  });
});

// The AddonsChild needs to be rooted so that it stays alive as long as
// the tab.
var AddonsChild = RemoteAddonsChild.init(this);
if (AddonsChild) {
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

addMessageListener("NetworkPrioritizer:SetPriority", (msg) => {
  let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
  let loadGroup = webNav.QueryInterface(Ci.nsIDocumentLoader)
                        .loadGroup.QueryInterface(Ci.nsISupportsPriority);
  loadGroup.priority = msg.data.priority;
});

var AutoCompletePopup = {
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

    addEventListener("unload", function() {
      AutoCompletePopup.destroy();
    });
  },

  destroy: function() {
    let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                       .getService(Ci.nsIFormFillController);

    controller.detachFromBrowser(docShell);
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
    if (!this._popupOpen) {
      // The search itself normally opens the popup itself, but in some cases,
      // nsAutoCompleteController tries to use cached results so notify our
      // popup to reuse the last results.
      sendAsyncMessage("FormAutoComplete:MaybeOpenPopup", {});
    }
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

addMessageListener("InPermitUnload", msg => {
  let inPermitUnload = docShell.contentViewer && docShell.contentViewer.inPermitUnload;
  sendAsyncMessage("InPermitUnload", {id: msg.data.id, inPermitUnload});
});

addMessageListener("PermitUnload", msg => {
  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "start"});

  let permitUnload = true;
  if (docShell && docShell.contentViewer) {
    permitUnload = docShell.contentViewer.permitUnload();
  }

  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "end", permitUnload});
});

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = content.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils)
                           .outerWindowID;
sendAsyncMessage("Browser:Init", {outerWindowID: outerWindowID});
addMessageListener("Browser:InitReceived", function onInitReceived(msg) {
  removeMessageListener("Browser:InitReceived", onInitReceived);
  if (msg.data.initPopup) {
    AutoCompletePopup.init();
  }
});
