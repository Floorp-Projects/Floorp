/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

this.ClearDataService = function() {};

ClearDataService.prototype = Object.freeze({
  classID: Components.ID("{0c06583d-7dd8-4293-b1a5-912205f779aa}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIClearDataService]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(ClearDataService),

  deleteDataFromHost(aHost, aIsUserRequest, aFlags, aCallback) {
    // TODO
  },

  deleteDataFromPrincipal(aPrincipal, aIsUserRequest, aFlags, aCallback) {
    // TODO
  },

  deleteDataInTimeRange(aFrom, aTo, aIsUserRequest, aFlags, aCallback) {
    // TODO
  },

  deleteData(aFlags, aCallback) {
    // TODO
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ClearDataService]);
