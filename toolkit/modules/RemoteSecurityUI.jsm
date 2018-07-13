// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteSecurityUI"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function RemoteSecurityUI() {
    this._secInfo = null;
    this._state = 0;
}

RemoteSecurityUI.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISecureBrowserUI]),

  // nsISecureBrowserUI
  get state() { return this._state; },
  get tooltipText() { return ""; },
  get secInfo() { return this._secInfo; },

  _update(aSecInfo, aState) {
    this._secInfo = aSecInfo;
    this._state = aState;
  }
};
