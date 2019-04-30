/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebNavigationChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");

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
      case "WebNavigation:LoadURI":
        let histogram = Services.telemetry.getKeyedHistogramById("FX_TAB_REMOTE_NAVIGATION_DELAY_MS");
        histogram.add("WebNavigation:LoadURI",
                      Services.telemetry.msSystemNow() - message.data.requestTime);

        this.loadURI(message.data);

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
  }

  _wrapURIChangeCall(fn) {
    this.mm.WebProgress.inLoadURI = true;
    try {
      fn();
    } finally {
      this.mm.WebProgress.inLoadURI = false;
      this.mm.WebProgress.sendLoadCallResult();
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
    let {
      index,
      cancelContentJSEpoch,
    } = params || {};
    this.mm.docShell.setCancelContentJSEpoch(cancelContentJSEpoch);
    this._wrapURIChangeCall(() => this.webNavigation.gotoIndex(index));
  }

  loadURI(params) {
    let {
      uri,
      flags,
      referrerInfo,
      postData,
      headers,
      baseURI,
      triggeringPrincipal,
      csp,
      cancelContentJSEpoch,
    } = params || {};

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
    if (postData)
      postData = E10SUtils.makeInputStream(postData);
    if (headers)
      headers = E10SUtils.makeInputStream(headers);
    if (baseURI)
      baseURI = Services.io.newURI(baseURI);
    this._assert(triggeringPrincipal, "We need a triggering principal to continue loading", new Error().lineNumber);

    triggeringPrincipal = E10SUtils.deserializePrincipal(triggeringPrincipal, () => {
      this._assert(false, "Unable to deserialize passed triggering principal", new Error().lineNumber);
      return Services.scriptSecurityManager.getSystemPrincipal({});
    });
    if (csp) {
      csp = E10SUtils.deserializeCSP(csp);
    }

    let loadURIOptions = {
      triggeringPrincipal,
      csp,
      loadFlags: flags,
      referrerInfo: E10SUtils.deserializeReferrerInfo(referrerInfo),
      postData,
      headers,
      baseURI,
    };
    this.mm.docShell.setCancelContentJSEpoch(cancelContentJSEpoch);
    this._wrapURIChangeCall(() => {
      return this.webNavigation.loadURI(uri, loadURIOptions);
    });
  }

  _assert(condition, msg, line = 0) {
    let debug = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
    if (!condition && debug.isDebugBuild) {
      debug.warning(`${msg} - ${new Error().stack}`, "WebNavigationChild.js", line);
      debug.abort("WebNavigationChild.js", line);
    }
  }

  setOriginAttributes(originAttributes) {
    if (originAttributes) {
      this.webNavigation.setOriginAttributesBeforeLoading(originAttributes);
    }
  }

  reload(flags) {
    this.webNavigation.reload(flags);
  }

  stop(flags) {
    this.webNavigation.stop(flags);
  }
}
