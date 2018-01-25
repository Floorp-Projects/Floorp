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
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    Cu.import("resource://gre/modules/AndroidLog.jsm",
              {}).AndroidLog.d.bind(null, "ViewNavigation"));

function debug(aMsg) {
  // dump(aMsg);
}

// Handles navigation requests between Gecko and a GeckoView.
// Handles GeckoView:GoBack and :GoForward requests dispatched by
// GeckoView.goBack and .goForward.
// Dispatches GeckoView:LocationChange to the GeckoView on location change when
// active.
// Implements nsIBrowserDOMWindow.
// Implements nsILoadURIDelegate.
class GeckoViewNavigation extends GeckoViewModule {
  init() {
    this.window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = this;
    this.browser.docShell.loadURIDelegate = this;

    this.eventDispatcher.registerListener(this, [
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

  handleLoadUri(aUri, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    debug("handleOpenURI: aUri=" + (aUri && aUri.spec) +
          " aWhere=" + aWhere +
          " aFlags=" + aFlags);

    if (!this.isRegistered) {
      return false;
    }

    let message = {
      type: "GeckoView:OnLoadUri",
      uri: aUri ? aUri.displaySpec : "",
      where: aWhere,
      flags: aFlags
    };

    debug("dispatch " + JSON.stringify(message));

    let handled = undefined;
    this.eventDispatcher.sendRequestForResult(message).then(response => {
      handled = response;
    });
    Services.tm.spinEventLoopUntil(() => handled !== undefined);

    return handled;
  }

  // nsILoadURIDelegate.
  loadURI(aUri, aWhere, aFlags, aTriggeringPrincipal) {
    debug("loadURI " + aUri + " " + aWhere + " " + aFlags + " " +
          aTriggeringPrincipal);

    let handled = this.handleLoadUri(aUri, null, aWhere, aFlags,
                                     aTriggeringPrincipal);
    if (!handled) {
      throw Cr.NS_ERROR_ABORT;
    }
  }

  // nsIBrowserDOMWindow.
  createContentWindow(aUri, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    debug("createContentWindow: aUri=" + (aUri && aUri.spec) +
          " aWhere=" + aWhere +
          " aFlags=" + aFlags);

    let handled = this.handleLoadUri(aUri, aOpener, aWhere, aFlags,
                                     aTriggeringPrincipal);
    if (!handled &&
        (aWhere === Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW ||
         aWhere === Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW)) {
      return this.browser.contentWindow;
    }

    throw Cr.NS_ERROR_ABORT;
  }

  // nsIBrowserDOMWindow.
  createContentWindowInFrame(aUri, aParams, aWhere, aFlags, aNextTabParentId,
                             aName) {
    debug("createContentWindowInFrame: aUri=" + (aUri && aUri.spec) +
          " aParams=" + aParams +
          " aWhere=" + aWhere +
          " aFlags=" + aFlags +
          " aNextTabParentId=" + aNextTabParentId +
          " aName=" + aName);

    let handled = this.handleLoadUri(aUri, null, aWhere, aFlags, null);
    if (!handled &&
        (aWhere === Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW ||
         aWhere === Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW)) {
      return this.browser;
    }

    throw Cr.NS_ERROR_ABORT;
  }

  // nsIBrowserDOMWindow.
  openURI(aUri, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    return this.createContentWindow(aUri, aOpener, aWhere, aFlags,
                                    aTriggeringPrincipal);
  }

  // nsIBrowserDOMWindow.
  openURIInFrame(aUri, aParams, aWhere, aFlags, aNextTabParentId, aName) {
    return this.createContentWindowInFrame(aUri, aParams, aWhere, aFlags,
                                           aNextTabParentId, aName);
  }

  // nsIBrowserDOMWindow.
  isTabContentWindow(aWindow) {
    debug("isTabContentWindow " + this.browser.contentWindow === aWindow);
    return this.browser.contentWindow === aWindow;
  }

  // nsIBrowserDOMWindow.
  canClose() {
    debug("canClose");
    return false;
  }

  register() {
    debug("register");

    let flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  unregister() {
    debug("unregister");

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
      fixedURI = Services.uriFixup.createExposableURI(aLocationURI);
    } catch (ex) { }

    let message = {
      type: "GeckoView:LocationChange",
      uri: fixedURI.displaySpec,
      canGoBack: this.browser.canGoBack,
      canGoForward: this.browser.canGoForward,
    };

    debug("dispatch " + JSON.stringify(message));

    this.eventDispatcher.sendRequest(message);
  }
}
