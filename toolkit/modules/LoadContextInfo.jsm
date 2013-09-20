/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["LoadContextInfo"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

this.LoadContextInfo = {};

_LoadContextInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsILoadContextInfo, Ci.nsISupports]),
  get isPrivate() { return this._isPrivate },
  get isAnonymous() { return this._isAnonymous },
  get isInBrowserElement() { return this._isInBrowserElement },
  get appId() { return this._appId }
}

function _LoadContextInfo(_private, _anonymous, _appId, _inBrowser) {
  this._isPrivate = _private || false;
  this._isAnonymous = _anonymous || false;
  this._appId = _appId || 0;
  this._isInBrowserElement = _inBrowser || false;
}

// LoadContextInfo.default

// Non-private, non-anonymous, no app ID, not in browser
XPCOMUtils.defineLazyGetter(LoadContextInfo, "default", function () {
  return new _LoadContextInfo(false, false, 0, false);
});

// LoadContextInfo.private

// Private, non-anonymous, no app ID, not in browser
XPCOMUtils.defineLazyGetter(LoadContextInfo, "private", function () {
  return new _LoadContextInfo(true, false, 0, false);
});

// LoadContextInfo.anonymous

// Non-private, anonymous, no app ID, not in browser
XPCOMUtils.defineLazyGetter(LoadContextInfo, "anonymous", function () {
  return new _LoadContextInfo(false, true, 0, false);
});

// Fully customizable
LoadContextInfo.custom = function(_private, _anonymous, _appId, _inBrowser) {
  return new _LoadContextInfo(_private, _anonymous, _appId, _inBrowser);
}

// Copies info from provided nsILoadContext
LoadContextInfo.fromLoadContext = function(_loadContext, _anonymous) {
  return new _LoadContextInfo(_loadContext.isPrivate,
                              _anonymous,
                              _loadContext.appId,
                              _loadContext.isInBrowserElement);
}
