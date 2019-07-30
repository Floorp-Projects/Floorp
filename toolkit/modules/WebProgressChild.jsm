/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebProgressChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "serializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class WebProgressChild {
  constructor(mm) {
    this.mm = mm;

    // NOTIFY_PROGRESS, NOTIFY_STATE_ALL, NOTIFY_STATUS, NOTIFY_LOCATION, NOTIFY_REFRESH, and
    // NOTIFY_CONTENT_BLOCKING are handled by PBrowser.
    let notifyCode =
      Ci.nsIWebProgress.NOTIFY_ALL &
      ~Ci.nsIWebProgress.NOTIFY_STATE_ALL &
      ~Ci.nsIWebProgress.NOTIFY_PROGRESS &
      ~Ci.nsIWebProgress.NOTIFY_STATUS &
      ~Ci.nsIWebProgress.NOTIFY_LOCATION &
      ~Ci.nsIWebProgress.NOTIFY_REFRESH &
      ~Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING;

    this._filter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, notifyCode);
    this._filter.target = this.mm.tabEventTarget;

    let webProgress = this.mm.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, notifyCode);
  }

  _requestSpec(aRequest, aPropertyName) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel)) {
      return null;
    }
    return aRequest[aPropertyName].spec;
  }

  _setupJSON(aWebProgress, aRequest) {
    if (aWebProgress) {
      let domWindowID = null;
      try {
        domWindowID = aWebProgress.DOMWindowID;
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
    };
  }

  _send(name, data) {
    this.mm.sendAsyncMessage(name, data);
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

    if (aWebProgress.isTopLevel) {
      json.secInfo = this.getSecInfoAsString();
      json.isSecureContext = this.mm.content.isSecureContext;
    }

    this._send("Content:SecurityChange", json);
  }
}

WebProgressChild.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIWebProgressListener",
  "nsISupportsWeakReference",
]);
