/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Using multiprocessShims is optional, and if an add-on is e10s compatible it should not
 * use it. But in some cases we still want to use the interposition service for various
 * features so we have a default shim service.
 */

function DefaultInterpositionService() {
}

DefaultInterpositionService.prototype = {
  classID: Components.ID("{50bc93ce-602a-4bef-bf3a-61fc749c4caf}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAddonInterposition, Ci.nsISupportsWeakReference]),

  getWhitelist() {
    return [];
  },

  interposeProperty(addon, target, iid, prop) {
    return null;
  },

  interposeCall(addonId, originalFunc, originalThis, args) {
    args.splice(0, 0, addonId);
    return originalFunc.apply(originalThis, args);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DefaultInterpositionService]);
