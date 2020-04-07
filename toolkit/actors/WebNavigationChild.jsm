/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebNavigationChild"];

class WebNavigationChild extends JSWindowActorChild {
  get webNavigation() {
    return this.docShell.QueryInterface(Ci.nsIWebNavigation);
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
      this.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIBrowserChild)
        .notifyNavigationFinished();
    }
  }

  goBack(params) {
    let wn = this.webNavigation;
    if (wn.canGoBack) {
      this.docShell.setCancelContentJSEpoch(params.cancelContentJSEpoch);
      this._wrapURIChangeCall(() => wn.goBack());
    }
  }

  goForward(params) {
    let wn = this.webNavigation;
    if (wn.canGoForward) {
      this.docShell.setCancelContentJSEpoch(params.cancelContentJSEpoch);
      this._wrapURIChangeCall(() => wn.goForward());
    }
  }

  gotoIndex(params) {
    let { index, cancelContentJSEpoch } = params || {};
    this.docShell.setCancelContentJSEpoch(cancelContentJSEpoch);
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
      debug.abort("WebNavigationChild.jsm", line);
    }
  }

  reload(loadFlags) {
    this.webNavigation.reload(loadFlags);
  }

  stop(loadFlags) {
    this.webNavigation.stop(loadFlags);
  }
}
