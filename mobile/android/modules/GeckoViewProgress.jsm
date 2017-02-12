/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewProgress"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewNavigation");

var DEBUG = false;

class GeckoViewProgress extends GeckoViewModule {
  init() {
    this.window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = this;

    this.registerProgressListener();
  }

  registerProgressListener() {
    if (DEBUG) {
      dump("registerProgressListeners()");
    }

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK | Ci.nsIWebProgress.NOTIFY_SECURITY;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (DEBUG) {
      dump("onStateChange()");
    }

    if (!aWebProgress.isTopLevel) {
      return;
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      let uri = aRequest.QueryInterface(Ci.nsIChannel).URI;
      let message = {
        type: "GeckoView:PageStart",
        uri: uri.spec,
      };

      this.eventDispatcher.sendRequest(message);
    } else if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) && !aWebProgress.isLoadingDocument) {
      let message = {
        type: "GeckoView:PageStop",
        success: aStatus ? false : true
      };

      this.eventDispatcher.sendRequest(message);
    }
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    if (DEBUG) {
      dump("onSecurityChange()");
    }

    let message = {
      type: "GeckoView:SecurityChanged",
      status: aState
    };

    this.eventDispatcher.sendRequest(message);
  }
}
