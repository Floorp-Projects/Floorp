// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteSecurityUI"];

function RemoteSecurityUI() {
  this._secInfo = null;
  this._state = 0;
  this._event = 0;
  this._isSecureContext = false;
}

RemoteSecurityUI.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISecureBrowserUI]),

  // nsISecureBrowserUI
  get state() {
    return this._state;
  },
  get contentBlockingEvent() {
    return this._event;
  },
  get secInfo() {
    return this._secInfo;
  },
  get isSecureContext() {
    return this._isSecureContext;
  },

  _update(aSecInfo, aState, aIsSecureContext) {
    this._secInfo = aSecInfo;
    this._state = aState;
    this._isSecureContext = aIsSecureContext;
  },
  _updateContentBlockingEvent(aEvent) {
    this._event = aEvent;
  },
};
