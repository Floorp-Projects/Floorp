// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteWebNavigation"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function makeURI(url)
{
  return Cc["@mozilla.org/network/io-service;1"].
         getService(Ci.nsIIOService).
         newURI(url, null, null);
}

function RemoteWebNavigation(browser)
{
  this._browser = browser;
  this._browser.messageManager.addMessageListener("WebNavigation:setHistory", this);
}

RemoteWebNavigation.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebNavigation, Ci.nsISupports]),

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
  goBack: function() {
    this._sendMessage("WebNavigation:GoBack", {});
  },
  goForward: function() {
    this._sendMessage("WebNavigation:GoForward", {});
  },
  gotoIndex: function(aIndex) {
    this._sendMessage("WebNavigation:GotoIndex", {index: aIndex});
  },
  loadURI: function(aURI, aLoadFlags, aReferrer, aPostData, aHeaders) {
    this._browser._contentTitle = "";
    this._sendMessage("WebNavigation:LoadURI", {uri: aURI, flags: aLoadFlags});
  },
  reload: function(aReloadFlags) {
    this._sendMessage("WebNavigation:Reload", {flags: aReloadFlags});
  },
  stop: function(aStopFlags) {
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

  _sessionHistory: null,
  get sessionHistory() { return this._sessionHistory; },
  set sessionHistory(aValue) { },

  _sendMessage: function(aMessage, aData) {
    try {
      this._browser.messageManager.sendAsyncMessage(aMessage, aData);
    }
    catch (e) {
      Cu.reportError(e);
    }
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "WebNavigation:setHistory":
        this._sessionHistory = aMessage.objects.history;
        break;
    }
  }
};
