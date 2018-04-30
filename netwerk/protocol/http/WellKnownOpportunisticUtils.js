/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const WELLKNOWNOPPORTUNISTICUTILS_CONTRACTID = "@mozilla.org/network/well-known-opportunistic-utils;1";
const WELLKNOWNOPPORTUNISTICUTILS_CID = Components.ID("{b4f96c89-5238-450c-8bda-e12c26f1d150}");

function WellKnownOpportunisticUtils() {
  this.valid = false;
  this.mixed = false;
  this.lifetime = 0;
}

WellKnownOpportunisticUtils.prototype = {
  classID: WELLKNOWNOPPORTUNISTICUTILS_CID,
  contractID: WELLKNOWNOPPORTUNISTICUTILS_CONTRACTID,
  classDescription: "Well-Known Opportunistic Utils",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWellKnownOpportunisticUtils]),

    verify: function(aJSON, aOrigin, aAlternatePort) {
	try {
	  let obj = JSON.parse(aJSON.toLowerCase());
	  let ports = obj[aOrigin.toLowerCase()]['tls-ports'];
	  if (!ports.includes(aAlternatePort)) {
	    throw "invalid port";
	  }
	  this.lifetime = obj[aOrigin.toLowerCase()]['lifetime'];
          this.mixed = obj[aOrigin.toLowerCase()]['mixed-scheme'];
	} catch (e) {
	  return;
	}
      this.valid = true;
    },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WellKnownOpportunisticUtils]);
