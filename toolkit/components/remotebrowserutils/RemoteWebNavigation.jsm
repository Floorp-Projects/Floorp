// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://gre/modules/sessionstore/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm");

function makeURI(url) {
  return Services.io.newURI(url);
}

function RemoteWebNavigation() {
  this.wrappedJSObject = this;
  this._cancelContentJSEpoch = 1;
}

RemoteWebNavigation.prototype = {
  classDescription: "nsIWebNavigation for remote browsers",
  classID: Components.ID("{4b56964e-cdf3-4bb8-830c-0e2dad3f4ebd}"),
  contractID: "@mozilla.org/remote-web-navigation;1",

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebNavigation]),

  swapBrowser(aBrowser) {
    this._browser = aBrowser;
  },

  LOAD_FLAGS_MASK: 65535,
  LOAD_FLAGS_NONE: 0,
  LOAD_FLAGS_IS_REFRESH: 16,
  LOAD_FLAGS_IS_LINK: 32,
  LOAD_FLAGS_BYPASS_HISTORY: 64,
  LOAD_FLAGS_REPLACE_HISTORY: 128,
  LOAD_FLAGS_BYPASS_CACHE: 256,
  LOAD_FLAGS_BYPASS_PROXY: 512,
  LOAD_FLAGS_CHARSET_CHANGE: 1024,
  LOAD_FLAGS_STOP_CONTENT: 2048,
  LOAD_FLAGS_FROM_EXTERNAL: 4096,
  LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP: 8192,
  LOAD_FLAGS_FIRST_LOAD: 16384,
  LOAD_FLAGS_ALLOW_POPUPS: 32768,
  LOAD_FLAGS_BYPASS_CLASSIFIER: 65536,
  LOAD_FLAGS_FORCE_ALLOW_COOKIES: 131072,

  STOP_NETWORK: 1,
  STOP_CONTENT: 2,
  STOP_ALL: 3,

  canGoBack: false,
  canGoForward: false,
  goBack() {
    let cancelContentJSEpoch = this._cancelContentJSEpoch++;
    this._browser.frameLoader.remoteTab.maybeCancelContentJSExecution(
      Ci.nsIRemoteTab.NAVIGATE_BACK, {epoch: cancelContentJSEpoch});
    this._sendMessage("WebNavigation:GoBack", {cancelContentJSEpoch});
  },
  goForward() {
    let cancelContentJSEpoch = this._cancelContentJSEpoch++;
    this._browser.frameLoader.remoteTab.maybeCancelContentJSExecution(
      Ci.nsIRemoteTab.NAVIGATE_FORWARD, {epoch: cancelContentJSEpoch});
    this._sendMessage("WebNavigation:GoForward", {cancelContentJSEpoch});
  },
  gotoIndex(aIndex) {
    let cancelContentJSEpoch = this._cancelContentJSEpoch++;
    this._browser.frameLoader.remoteTab.maybeCancelContentJSExecution(
      Ci.nsIRemoteTab.NAVIGATE_INDEX,
      {index: aIndex, epoch: cancelContentJSEpoch});
    this._sendMessage("WebNavigation:GotoIndex", {index: aIndex,
                                                  cancelContentJSEpoch});
  },
  loadURI(aURI, aLoadURIOptions) {
    let uri;

    // We know the url is going to be loaded, let's start requesting network
    // connection before the content process asks.
    // Note that we might have already setup the speculative connection in some
    // cases, especially when the url is from location bar or its popup menu.
    if (aURI.startsWith("http:") || aURI.startsWith("https:")) {
      try {
        uri = makeURI(aURI);
        let principal = aLoadURIOptions.triggeringPrincipal;
        // We usually have a triggeringPrincipal assigned, but in case we
        // don't have one or if it's a SystemPrincipal, let's create it with OA
        // inferred from the current context.
        if (!principal || principal.isSystemPrincipal) {
          let attrs = {
            userContextId: this._browser.getAttribute("usercontextid") || 0,
            privateBrowsingId: PrivateBrowsingUtils.isBrowserPrivate(this._browser) ? 1 : 0,
          };
          principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, attrs);
        }
        Services.io.speculativeConnect(uri, principal, null);
      } catch (ex) {
        // Can't setup speculative connection for this uri string for some
        // reason (such as failing to parse the URI), just ignore it.
      }
    }

    let cancelContentJSEpoch = this._cancelContentJSEpoch++;
    this._browser.frameLoader.remoteTab.maybeCancelContentJSExecution(
      Ci.nsIRemoteTab.NAVIGATE_URL, {uri, epoch: cancelContentJSEpoch});
    this._sendMessage("WebNavigation:LoadURI", {
      uri: aURI,
      flags: aLoadURIOptions.loadFlags,
      referrerInfo: E10SUtils.serializeReferrerInfo(aLoadURIOptions.referrerInfo),
      postData: aLoadURIOptions.postData ? Utils.serializeInputStream(aLoadURIOptions.postData) : null,
      headers: aLoadURIOptions.headers ? Utils.serializeInputStream(aLoadURIOptions.headers) : null,
      baseURI: aLoadURIOptions.baseURI ? aLoadURIOptions.baseURI.spec : null,
      triggeringPrincipal: E10SUtils.serializePrincipal(
                           aLoadURIOptions.triggeringPrincipal || Services.scriptSecurityManager.createNullPrincipal({})),
      csp: aLoadURIOptions.csp ? E10SUtils.serializeCSP(aLoadURIOptions.csp) : null,
      requestTime: Services.telemetry.msSystemNow(),
      cancelContentJSEpoch,
    });
  },
  setOriginAttributesBeforeLoading(aOriginAttributes) {
    this._sendMessage("WebNavigation:SetOriginAttributes", {
      originAttributes: aOriginAttributes,
    });
  },
  reload(aReloadFlags) {
    this._sendMessage("WebNavigation:Reload", {flags: aReloadFlags});
  },
  stop(aStopFlags) {
    this._sendMessage("WebNavigation:Stop", {flags: aStopFlags});
  },

  get document() {
    return this._browser.contentDocument;
  },

  _currentURI: null,
  get currentURI() {
    if (!this._currentURI) {
      this._currentURI = makeURI("about:blank");
    }

    return this._currentURI;
  },
  set currentURI(aURI) {
    // Bug 1498600 verify usages of systemPrincipal here
    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    this.loadURI(aURI.spec, loadURIOptions);
  },

  referringURI: null,

  // Bug 1233803 - accessing the sessionHistory of remote browsers should be
  // done in content scripts.
  get sessionHistory() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  set sessionHistory(aValue) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  _sendMessage(aMessage, aData) {
    try {
      this._browser.messageManager.sendAsyncMessage(aMessage, aData);
    } catch (e) {
      Cu.reportError(e);
    }
  },
};

var EXPORTED_SYMBOLS = ["RemoteWebNavigation"];
