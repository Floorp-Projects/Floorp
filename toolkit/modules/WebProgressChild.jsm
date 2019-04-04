/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebProgressChild"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");

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

    // NOTIFY_PROGRESS, NOTIFY_STATUS, NOTIFY_REFRESH, and
    // NOTIFY_CONTENT_BLOCKING are handled by PBrowser.
    let notifyCode = Ci.nsIWebProgress.NOTIFY_ALL &
                        ~Ci.nsIWebProgress.NOTIFY_PROGRESS &
                        ~Ci.nsIWebProgress.NOTIFY_STATUS &
                        ~Ci.nsIWebProgress.NOTIFY_REFRESH &
                        ~Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING;

    this._filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, notifyCode);
    this._filter.target = this.mm.tabEventTarget;

    let webProgress = this.mm.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, notifyCode);

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
      // After Bug 965637 we can query the csp directly from content.document
      // instead of content.document.nodePrincipal.
      let csp = this.mm.content.document.nodePrincipal.csp;
      json.csp = E10SUtils.serializeCSP(csp);
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

  sendLoadCallResult() {
    this.mm.sendAsyncMessage("Content:LoadURIResult");
  }
}

WebProgressChild.prototype.QueryInterface =
  ChromeUtils.generateQI(["nsIWebProgressListener",
                          "nsISupportsWeakReference"]);
