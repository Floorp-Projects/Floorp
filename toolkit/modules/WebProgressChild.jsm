/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebProgressChild"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
XPCOMUtils.defineLazyServiceGetter(this, "serializationHelper",
                                   "@mozilla.org/network/serialization-helper;1",
                                   "nsISerializationHelper");

class WebProgressChild {
  constructor(mm) {
    this.mm = mm;

    this.inLoadURI = false;

    this._filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);
    this._filter.target = this.mm.tabEventTarget;

    let webProgress = this.mm.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, Ci.nsIWebProgress.NOTIFY_ALL);

    // This message is used for measuring this.mm.content process startup performance.
    this.mm.sendAsyncMessage("Content:BrowserChildReady", { time: Services.telemetry.msSystemNow() });
  }

  _requestSpec(aRequest, aPropertyName) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel))
      return null;
    return aRequest[aPropertyName].spec;
  }

  _setupJSON(aWebProgress, aRequest, aStateFlags) {
    // Avoid accessing this.mm.content.document when being called from onStateChange
    // unless if we are in STATE_STOP, because otherwise the getter will
    // instantiate an about:blank document for us.
    let contentDocument = null;
    if (aStateFlags) {
      // We're being called from onStateChange
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        contentDocument = this.mm.content.document;
      }
    } else {
      contentDocument = this.mm.content.document;
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
        DOMWindowID: domWindowID,
      };
    }

    return {
      webProgress: aWebProgress || null,
      requestURI: this._requestSpec(aRequest, "URI"),
      originalRequestURI: this._requestSpec(aRequest, "originalURI"),
      documentContentType: contentDocument ? contentDocument.contentType : null,
      innerWindowID,
    };
  }

  _send(name, data) {
    this.mm.sendAsyncMessage(name, data);
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let json = this._setupJSON(aWebProgress, aRequest, aStateFlags);

    json.stateFlags = aStateFlags;
    json.status = aStatus;

    // It's possible that this state change was triggered by
    // loading an internal error page, for which the parent
    // will want to know some details, so we'll update it with
    // the documentURI.
    if (aWebProgress && aWebProgress.isTopLevel) {
      json.documentURI = this.mm.content.document.documentURIObject.spec;
      json.charset = this.mm.content.document.characterSet;
      json.mayEnableCharacterEncodingMenu = this.mm.docShell.mayEnableCharacterEncodingMenu;
      json.inLoadURI = this.inLoadURI;
    }

    this._send("Content:StateChange", json);
  }

  // Note: Because the nsBrowserStatusFilter timeout runnable is
  // SystemGroup-labeled, this method should not modify this.mm.content DOM or
  // run this.mm.content JS.
  onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    let json = this._setupJSON(aWebProgress, aRequest);

    json.curSelf = aCurSelf;
    json.maxSelf = aMaxSelf;
    json.curTotal = aCurTotal;
    json.maxTotal = aMaxTotal;

    this._send("Content:ProgressChange", json);
  }

  onProgressChange64(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    this.onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal);
  }

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    let json = this._setupJSON(aWebProgress, aRequest);

    json.location = aLocationURI ? aLocationURI.spec : "";
    json.flags = aFlags;

    // These properties can change even for a sub-frame navigation.
    let webNav = this.mm.docShell.QueryInterface(Ci.nsIWebNavigation);
    json.canGoBack = webNav.canGoBack;
    json.canGoForward = webNav.canGoForward;

    if (aWebProgress && aWebProgress.isTopLevel) {
      json.documentURI = this.mm.content.document.documentURIObject.spec;
      json.title = this.mm.content.document.title;
      json.charset = this.mm.content.document.characterSet;
      json.mayEnableCharacterEncodingMenu = this.mm.docShell.mayEnableCharacterEncodingMenu;
      json.principal = this.mm.content.document.nodePrincipal;
      json.synthetic = this.mm.content.document.mozSyntheticDocument;
      json.inLoadURI = this.inLoadURI;
      json.requestContextID = this.mm.content.document.documentLoadGroup
        ? this.mm.content.document.documentLoadGroup.requestContextID
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

    this._send("Content:LocationChange", json);
  }

  // Note: Because the nsBrowserStatusFilter timeout runnable is
  // SystemGroup-labeled, this method should not modify this.mm.content DOM or
  // run this.mm.content JS.
  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = this._setupJSON(aWebProgress, aRequest);

    json.status = aStatus;
    json.message = aMessage;

    this._send("Content:StatusChange", json);
  }

  getSecInfoAsString() {
    let secInfo = this.mm.docShell.securityUI.secInfo;
    if (secInfo) {
      return serializationHelper.serializeToString(secInfo);
    }

    return null;
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    let json = this._setupJSON(aWebProgress, aRequest);

    json.state = aState;
    json.secInfo = this.getSecInfoAsString();

    this._send("Content:SecurityChange", json);
  }

  onContentBlockingEvent(aWebProgress, aRequest, aEvent) {
    let json = this._setupJSON(aWebProgress, aRequest);

    json.event = aEvent;
    json.matchedList = null;
    if (aRequest && aRequest instanceof Ci.nsIClassifiedChannel) {
      json.matchedList = aRequest.matchedList;
    }

    this._send("Content:ContentBlockingEvent", json);
  }

  onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
    return true;
  }

  sendLoadCallResult() {
    this.mm.sendAsyncMessage("Content:LoadURIResult");
  }
}

WebProgressChild.prototype.QueryInterface =
  ChromeUtils.generateQI(["nsIWebProgressListener",
                          "nsIWebProgressListener2",
                          "nsISupportsWeakReference"]);
