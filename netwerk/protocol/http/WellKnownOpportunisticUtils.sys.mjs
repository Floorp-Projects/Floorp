/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export function WellKnownOpportunisticUtils() {
  this.valid = false;
  this.mixed = false;
  this.lifetime = 0;
}

WellKnownOpportunisticUtils.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIWellKnownOpportunisticUtils"]),

  verify(aJSON, aOrigin) {
    try {
      let arr = JSON.parse(aJSON.toLowerCase());
      if (!arr.includes(aOrigin.toLowerCase())) {
        throw new Error("invalid origin");
      }
    } catch (e) {
      return;
    }
    this.valid = true;
  },
};
