/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ResponsivenessMonitor"];

function ResponsivenessMonitor(intervalMS = 100) {
  this._intervalMS = intervalMS;
  this._prevTimestamp = Date.now();
  this._accumulatedDelay = 0;
  this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._timer.initWithCallback(
    this,
    this._intervalMS,
    Ci.nsITimer.TYPE_REPEATING_SLACK
  );
}

ResponsivenessMonitor.prototype = {
  notify() {
    let now = Date.now();
    this._accumulatedDelay += Math.max(
      0,
      now - this._prevTimestamp - this._intervalMS
    );
    this._prevTimestamp = now;
  },

  abort() {
    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }
  },

  finish() {
    this.abort();
    return this._accumulatedDelay;
  },
};
