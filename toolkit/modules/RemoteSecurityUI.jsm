// -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteSecurityUI"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function RemoteSecurityUI()
{
    this._SSLStatus = null;
    this._state = 0;
}

RemoteSecurityUI.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISSLStatusProvider, Ci.nsISecureBrowserUI]),

  // nsISSLStatusProvider
  get SSLStatus() { return this._SSLStatus; },

  // nsISecureBrowserUI
  get state() { return this._state; },
  get tooltipText() { return ""; },

  _update: function (aStatus, aState) {
    this._SSLStatus = aStatus;
    this._state = aState;
  }
};
