/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebNavigationChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
                               "resource://gre/modules/sessionstore/Utils.jsm");

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

  goBack() {
    if (this.webNavigation.canGoBack) {
      this._wrapURIChangeCall(() => this.webNavigation.goBack());
    }
  }

  goForward() {
    if (this.webNavigation.canGoForward) {
      this._wrapURIChangeCall(() => this.webNavigation.goForward());
    }
  }

  gotoIndex(index) {
    this._wrapURIChangeCall(() => this.webNavigation.gotoIndex(index));
  }

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
    if (!triggeringPrincipal) {
      triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal({});
    }
    this._wrapURIChangeCall(() => {
      return this.webNavigation.loadURIWithOptions(uri, flags, referrer, referrerPolicy,
                                                   postData, headers, baseURI, triggeringPrincipal);
    });
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
