/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function WellKnownOpportunisticUtils() {
  this.valid = false;
  this.mixed = false;
  this.lifetime = 0;
}

WellKnownOpportunisticUtils.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWellKnownOpportunisticUtils]),

  verify(aJSON, aOrigin, aAlternatePort) {
    try {
      let obj = JSON.parse(aJSON.toLowerCase());
      let ports = obj[aOrigin.toLowerCase()]["tls-ports"];
      if (!ports.includes(aAlternatePort)) {
        throw new Error("invalid port");
      }
      this.lifetime = obj[aOrigin.toLowerCase()].lifetime;
      this.mixed = obj[aOrigin.toLowerCase()]["mixed-scheme"];
    } catch (e) {
      return;
    }
    this.valid = true;
  },
};

var EXPORTED_SYMBOLS = ["WellKnownOpportunisticUtils"];
