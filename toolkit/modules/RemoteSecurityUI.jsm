// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteSecurityUI"];

function RemoteSecurityUI() {
    this._secInfo = null;
    this._oldState = 0;
    this._state = 0;
    this._contentBlockingLogJSON = "";
}

RemoteSecurityUI.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISecureBrowserUI]),

  // nsISecureBrowserUI
  get oldState() { return this._oldState; },
  get state() { return this._state; },
  get contentBlockingLogJSON() { return this._contentBlockingLogJSON; },
  get tooltipText() { return ""; },
  get secInfo() { return this._secInfo; },

  _update(aSecInfo, aOldState, aState, aContentBlockingLogJSON) {
    this._secInfo = aSecInfo;
    this._oldState = aOldState;
    this._state = aState;
    this._contentBlockingLogJSON = aContentBlockingLogJSON;
  },
};
