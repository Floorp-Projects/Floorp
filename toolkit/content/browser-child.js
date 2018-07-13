/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

ChromeUtils.defineModuleGetter(this, "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://gre/modules/sessionstore/Utils.jsm");

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
                                     "@mozilla.org/xre/app-info;1",
                                     "nsICrashReporter");
}

var WebProgressListener = {
  init() {
    this._filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);
    this._filter.target = tabEventTarget;

    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, Ci.nsIWebProgress.NOTIFY_ALL);
    this.init = null;
  },

  uninit() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this._filter);

    this._filter.removeProgressListener(this);
    this._filter = null;
  },

  _requestSpec(aRequest, aPropertyName) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel))
      return null;
    return aRequest.QueryInterface(Ci.nsIChannel)[aPropertyName].spec;
  },

  _setupJSON: function setupJSON(aWebProgress, aRequest, aStateFlags) {
    // Avoid accessing content.document when being called from onStateChange
    // unless if we are in STATE_STOP, because otherwise the getter will
    // instantiate an about:blank document for us.
    let contentDocument = null;
    if (aStateFlags) {
      // We're being called from onStateChange
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        contentDocument = content.document;
      }
    } else {
      contentDocument = content.document;
    }

    let innerWindowID = null;
    if (aWebProgress) {
      let domWindowID = null;
      try {
        domWindowID = aWebProgress.DOMWindowID;
        innerWindowID = aWebProgress.innerDOMWindowID;
      } catch (e) {
        // The DOM Window ID getters above may throw if the inner or outer
        // windows aren't created yet or are destroyed at the time we're making
        // this call but that isn't fatal so ignore the exceptions here.
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
      documentContentType: contentDocument ? contentDocument.contentType : null,
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
      contentDocument: content.document,
      // DOMWindow is not necessarily the content-window with subframes.
      DOMWindow: domWindow,
      webProgress: aWebProgress,
      request: aRequest,
    };
  },

  _send(name, data, objects) {
    sendAsyncMessage(name, data, objects);
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let json = this._setupJSON(aWebProgress, aRequest, aStateFlags);
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

  // Note: Because the nsBrowserStatusFilter timeout runnable is
  // SystemGroup-labeled, this method should not modify content DOM or
  // run content JS.
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
      json.requestContextID = content.document.documentLoadGroup
        ? content.document.documentLoadGroup.requestContextID
        : null;

      if (AppConstants.MOZ_CRASHREPORTER && CrashReporter.enabled) {
        let uri = aLocationURI;
        try {
          // If the current URI contains a username/password, remove it.
          uri = uri.mutate()
                   .setUserPass("")
                   .finalize();
        } catch (ex) { /* Ignore failures on about: URIs. */ }
        CrashReporter.annotateCrashReport("URL", uri.spec);
      }
    }

    this._send("Content:LocationChange", json, objects);
  },

  // Note: Because the nsBrowserStatusFilter timeout runnable is
  // SystemGroup-labeled, this method should not modify content DOM or
  // run content JS.
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
    json.secInfo = SecurityUI.getSecInfoAsString();

    json.matchedList = null;
    if (aRequest && aRequest instanceof Ci.nsIClassifiedChannel) {
      json.matchedList = aRequest.matchedList;
    }

    this._send("Content:SecurityChange", json, objects);
  },

  onRefreshAttempted: function onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
    return true;
  },

  sendLoadCallResult() {
    sendAsyncMessage("Content:LoadURIResult");
  },

  QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener",
                                          "nsIWebProgressListener2",
                                          "nsISupportsWeakReference"]),
};

WebProgressListener.init();
addEventListener("unload", () => {
  WebProgressListener.uninit();
});

var WebNavigation =  {
  init() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:SetOriginAttributes", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);
    // This message is used for measuring content process startup performance.
    sendAsyncMessage("Content:BrowserChildReady", { time: Services.telemetry.msSystemNow() });
    this.init = null;
  },

  get webNavigation() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
  },

  _inLoadURI: false,

  get inLoadURI() {
    return this._inLoadURI;
  },

  receiveMessage(message) {
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
        let histogram = Services.telemetry.getKeyedHistogramById("FX_TAB_REMOTE_NAVIGATION_DELAY_MS");
        histogram.add("WebNavigation:LoadURI",
                      Services.telemetry.msSystemNow() - message.data.requestTime);

        this.loadURI(message.data.uri, message.data.flags,
                     message.data.referrer, message.data.referrerPolicy,
                     message.data.postData, message.data.headers,
                     message.data.baseURI, message.data.triggeringPrincipal);
        break;
      case "WebNavigation:SetOriginAttributes":
        this.setOriginAttributes(message.data.originAttributes);
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

  goBack() {
    if (this.webNavigation.canGoBack) {
      this._wrapURIChangeCall(() => this.webNavigation.goBack());
    }
  },

  goForward() {
    if (this.webNavigation.canGoForward) {
      this._wrapURIChangeCall(() => this.webNavigation.goForward());
    }
  },

  gotoIndex(index) {
    this._wrapURIChangeCall(() => this.webNavigation.gotoIndex(index));
  },

  loadURI(uri, flags, referrer, referrerPolicy, postData, headers, baseURI, triggeringPrincipal) {
    if (AppConstants.MOZ_CRASHREPORTER && CrashReporter.enabled) {
      let annotation = uri;
      try {
        let url = Services.io.newURI(uri);
        // If the current URI contains a username/password, remove it.
        url = url.mutate()
                 .setUserPass("")
                 .finalize();
        annotation = url.spec;
      } catch (ex) { /* Ignore failures to parse and failures
                      on about: URIs. */ }
      CrashReporter.annotateCrashReport("URL", annotation);
    }
    if (referrer)
      referrer = Services.io.newURI(referrer);
    if (postData)
      postData = Utils.makeInputStream(postData);
    if (headers)
      headers = Utils.makeInputStream(headers);
    if (baseURI)
      baseURI = Services.io.newURI(baseURI);
    if (triggeringPrincipal)
      triggeringPrincipal = Utils.deserializePrincipal(triggeringPrincipal);
    this._wrapURIChangeCall(() => {
      return this.webNavigation.loadURIWithOptions(uri, flags, referrer, referrerPolicy,
                                                   postData, headers, baseURI, triggeringPrincipal);
    });
  },

  setOriginAttributes(originAttributes) {
    if (originAttributes) {
      this.webNavigation.setOriginAttributesBeforeLoading(originAttributes);
    }
  },

  reload(flags) {
    this.webNavigation.reload(flags);
  },

  stop(flags) {
    this.webNavigation.stop(flags);
  }
};

WebNavigation.init();

var SecurityUI = {
  getSecInfoAsString() {
    let secInfo = docShell.securityUI.secInfo;

    if (secInfo) {
      if (secInfo) {
        let helper = Cc["@mozilla.org/network/serialization-helper;1"]
                        .getService(Ci.nsISerializationHelper);

        secInfo.QueryInterface(Ci.nsISerializable);
        return helper.serializeToString(secInfo);
      }
    }

    return null;
  }
};

var ControllerCommands = {
  init() {
    addMessageListener("ControllerCommands:Do", this);
    addMessageListener("ControllerCommands:DoWithParams", this);
    this.init = null;
  },

  receiveMessage(message) {
    switch (message.name) {
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
};

ControllerCommands.init();

addEventListener("DOMTitleChanged", function(aEvent) {
  if (!aEvent.isTrusted || aEvent.target.defaultView != content)
    return;
  sendAsyncMessage("DOMTitleChanged", { title: content.document.title });
}, false);

addEventListener("DOMWindowClose", function(aEvent) {
  if (!aEvent.isTrusted)
    return;
  sendAsyncMessage("DOMWindowClose");
}, false);

addEventListener("ImageContentLoaded", function(aEvent) {
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

  refreshFullZoom() {
    return this._refreshZoomValue("fullZoom");
  },

  refreshTextZoom() {
    return this._refreshZoomValue("textZoom");
  },

  /**
   * Retrieves specified zoom property value from markupViewer and refreshes
   * cache if needed.
   * @param valueName Either 'fullZoom' or 'textZoom'.
   * @returns Returns true if cached value was actually refreshed.
   * @private
   */
  _refreshZoomValue(valueName) {
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

addMessageListener("FullZoom", function(aMessage) {
  ZoomManager.fullZoom = aMessage.data.value;
});

addMessageListener("TextZoom", function(aMessage) {
  ZoomManager.textZoom = aMessage.data.value;
});

addEventListener("FullZoomChange", function() {
  if (ZoomManager.refreshFullZoom()) {
    sendAsyncMessage("FullZoomChange", { value: ZoomManager.fullZoom });
  }
}, false);

addEventListener("TextZoomChange", function(aEvent) {
  if (ZoomManager.refreshTextZoom()) {
    sendAsyncMessage("TextZoomChange", { value: ZoomManager.textZoom });
  }
}, false);

addEventListener("ZoomChangeUsingMouseWheel", function() {
  sendAsyncMessage("ZoomChangeUsingMouseWheel", {});
}, false);

addMessageListener("UpdateCharacterSet", function(aMessage) {
  docShell.charset = aMessage.data.value;
  docShell.gatherCharsetMenuTelemetry();
});

/**
 * Remote thumbnail request handler for PageThumbs thumbnails.
 */
addMessageListener("Browser:Thumbnail:Request", function(aMessage) {
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

  snapshot.toBlob(function(aBlob) {
    sendAsyncMessage("Browser:Thumbnail:Response", {
      thumbnail: aBlob,
      id: aMessage.data.id
    });
  });
});

/**
 * Remote isSafeForCapture request handler for PageThumbs.
 */
addMessageListener("Browser:Thumbnail:CheckState", function(aMessage) {
  Services.tm.idleDispatchToMainThread(() => {
    let result = PageThumbUtils.shouldStoreContentThumbnail(content, docShell);
    sendAsyncMessage("Browser:Thumbnail:CheckState:Response", {
      result
    });
  });
});

/**
 * Remote GetOriginalURL request handler for PageThumbs.
 */
addMessageListener("Browser:Thumbnail:GetOriginalURL", function(aMessage) {
  let channel = docShell.currentDocumentChannel;
  let channelError = PageThumbUtils.isChannelErrorResponse(channel);
  let originalURL;
  try {
    originalURL = channel.originalURI.spec;
  } catch (ex) {}
  sendAsyncMessage("Browser:Thumbnail:GetOriginalURL:Response", {
    channelError,
    originalURL,
  });
});

/**
 * Remote createAboutBlankContentViewer request handler.
 */
addMessageListener("Browser:CreateAboutBlank", function(aMessage) {
  if (!content.document || content.document.documentURI != "about:blank") {
    throw new Error("Can't create a content viewer unless on about:blank");
  }
  let principal = aMessage.data;
  principal = BrowserUtils.principalWithMatchingOA(principal, content.document.nodePrincipal);
  docShell.createAboutBlankContentViewer(principal);
});

addMessageListener("InPermitUnload", msg => {
  let inPermitUnload = docShell.contentViewer && docShell.contentViewer.inPermitUnload;
  sendAsyncMessage("InPermitUnload", {id: msg.data.id, inPermitUnload});
});

addMessageListener("PermitUnload", msg => {
  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "start"});

  let permitUnload = true;
  if (docShell && docShell.contentViewer) {
    permitUnload = docShell.contentViewer.permitUnload(msg.data.aPermitUnloadFlags);
  }

  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "end", permitUnload});
});

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = content.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils)
                           .outerWindowID;
sendAsyncMessage("Browser:Init", {outerWindowID});
