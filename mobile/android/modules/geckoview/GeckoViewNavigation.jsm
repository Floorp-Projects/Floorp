/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewNavigation"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewNavigation");

function debug(aMsg) {
  // dump(aMsg);
}

// Handles navigation requests between Gecko and a GeckoView.
// Implements GeckoView.loadUri via openURI.
// Handles GeckoView:GoBack and :GoForward requests dispatched by
// GeckoView.goBack and .goForward.
// Dispatches GeckoView:LocationChange to the GeckoView on location change when
// active.
// Can be activated and deactivated by GeckoViewNavigation:Active and :Inactive
// events.
class GeckoViewNavigation extends GeckoViewModule {
  init() {
    this.window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = this;

    // We need to listen initially because the "GeckoViewNavigation:Active"
    // event may be dispatched before this object is constructed.
    this.registerProgressListener();

    this.eventDispatcher.registerListener(this, [
      "GeckoViewNavigation:Active",
      "GeckoViewNavigation:Inactive",
      "GeckoView:GoBack",
      "GeckoView:GoForward",
      "GeckoView:LoadUri",
      "GeckoView:Reload",
      "GeckoView:Stop"
    ]);
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug("onEvent: aEvent=" + aEvent + ", aData=" + JSON.stringify(aData));

    switch (aEvent) {
      case "GeckoViewNavigation:Active":
        this.registerProgressListener();
        break;
      case "GeckoViewNavigation:Inactive":
        this.unregisterProgressListener();
        break;
      case "GeckoView:GoBack":
        this.browser.goBack();
        break;
      case "GeckoView:GoForward":
        this.browser.goForward();
        break;
      case "GeckoView:LoadUri":
        this.browser.loadURI(aData.uri, null, null, null);
        break;
      case "GeckoView:Reload":
        this.browser.reload();
        break;
      case "GeckoView:Stop":
        this.browser.stop();
        break;
    }
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug("receiveMessage " + aMsg.name);
  }

  // nsIBrowserDOMWindow::openURI implementation.
  openURI(aUri, aOpener, aWhere, aFlags) {
    debug("openURI: aUri.spec=" + aUri.spec);
    // nsIWebNavigation::loadURI(URI, loadFlags, referrer, postData, headers).
    this.browser.loadURI(aUri.spec, null, null, null);
  }

  // nsIBrowserDOMWindow::canClose implementation.
  canClose() {
    debug("canClose");
    return false;
  }

  registerProgressListener() {
    debug("registerProgressListener");
    let flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  unregisterProgressListener() {
    debug("unregisterProgressListener");
    if (!this.progressFilter) {
      return;
    }
    this.progressFilter.removeProgressListener(this);
    this.browser.removeProgressListener(this.progressFilter);
  }

  // WebProgress event handler.
  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug("onLocationChange");

    let fixedURI = aLocationURI;

    try {
      fixedURI = URIFixup.createExposableURI(aLocationURI);
    } catch (ex) { }

    let message = {
      type: "GeckoView:LocationChange",
      uri: fixedURI.spec,
      canGoBack: this.browser.canGoBack,
      canGoForward: this.browser.canGoForward,
    };

    debug("dispatch " + JSON.stringify(message));

    this.eventDispatcher.sendRequest(message);
  }
}
