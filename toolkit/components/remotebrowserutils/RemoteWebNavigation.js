// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

function makeURI(url) {
  return Services.io.newURI(url, null, null);
}

function readInputStreamToString(aStream) {
  return NetUtil.readInputStreamToString(aStream, aStream.available());
}

function RemoteWebNavigation() {
  this.wrappedJSObject = this;
}

RemoteWebNavigation.prototype = {
  classDescription: "nsIWebNavigation for remote browsers",
  classID: Components.ID("{4b56964e-cdf3-4bb8-830c-0e2dad3f4ebd}"),
  contractID: "@mozilla.org/remote-web-navigation;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebNavigation, Ci.nsISupports]),

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
    this._sendMessage("WebNavigation:GoBack", {});
  },
  goForward() {
    this._sendMessage("WebNavigation:GoForward", {});
  },
  gotoIndex(aIndex) {
    this._sendMessage("WebNavigation:GotoIndex", {index: aIndex});
  },
  loadURI(aURI, aLoadFlags, aReferrer, aPostData, aHeaders) {
    this.loadURIWithOptions(aURI, aLoadFlags, aReferrer,
                            Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
                            aPostData, aHeaders, null);
  },
  loadURIWithOptions(aURI, aLoadFlags, aReferrer, aReferrerPolicy,
                               aPostData, aHeaders, aBaseURI) {
    this._sendMessage("WebNavigation:LoadURI", {
      uri: aURI,
      flags: aLoadFlags,
      referrer: aReferrer ? aReferrer.spec : null,
      referrerPolicy: aReferrerPolicy,
      postData: aPostData ? readInputStreamToString(aPostData) : null,
      headers: aHeaders ? readInputStreamToString(aHeaders) : null,
      baseURI: aBaseURI ? aBaseURI.spec : null,
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
    this.loadURI(aURI.spec, null, null, null);
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RemoteWebNavigation]);
