/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebNavigationChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

class WebNavigationChild extends ActorChild {
  get webNavigation() {
    return this.mm.docShell.QueryInterface(Ci.nsIWebNavigation);
  }

  receiveMessage(message) {
    switch (message.name) {
      case "WebNavigation:GoBack":
        this.goBack(message.data);
        break;
      case "WebNavigation:GoForward":
        this.goForward(message.data);
        break;
      case "WebNavigation:GotoIndex":
        this.gotoIndex(message.data);
        break;
      case "WebNavigation:SetOriginAttributes":
        this.setOriginAttributes(message.data.originAttributes);
        break;
      case "WebNavigation:Reload":
        this.reload(message.data.loadFlags);
        break;
      case "WebNavigation:Stop":
        this.stop(message.data.loadFlags);
        break;
    }
  }

  _wrapURIChangeCall(fn) {
    try {
      fn();
    } finally {
      this.mm.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIBrowserChild)
        .notifyNavigationFinished();
    }
  }

  goBack(params) {
    if (this.webNavigation.canGoBack) {
      this.mm.docShell.setCancelContentJSEpoch(params.cancelContentJSEpoch);
      this._wrapURIChangeCall(() => this.webNavigation.goBack());
    }
  }

  goForward(params) {
    if (this.webNavigation.canGoForward) {
      this.mm.docShell.setCancelContentJSEpoch(params.cancelContentJSEpoch);
      this._wrapURIChangeCall(() => this.webNavigation.goForward());
    }
  }

  gotoIndex(params) {
    let { index, cancelContentJSEpoch } = params || {};
    this.mm.docShell.setCancelContentJSEpoch(cancelContentJSEpoch);
    this._wrapURIChangeCall(() => this.webNavigation.gotoIndex(index));
  }

  _assert(condition, msg, line = 0) {
    let debug = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
    if (!condition && debug.isDebugBuild) {
      debug.warning(
        `${msg} - ${new Error().stack}`,
        "WebNavigationChild.js",
        line
      );
      debug.abort("WebNavigationChild.js", line);
    }
  }

  setOriginAttributes(originAttributes) {
    if (originAttributes) {
      this.webNavigation.setOriginAttributesBeforeLoading(originAttributes);
    }
  }

  reload(loadFlags) {
    this.webNavigation.reload(loadFlags);
  }

  stop(loadFlags) {
    this.webNavigation.stop(loadFlags);
  }
}
