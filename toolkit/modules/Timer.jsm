/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * JS module implementation of nsIDOMJSWindow.setTimeout and clearTimeout.
 */

this.EXPORTED_SYMBOLS = ["setTimeout", "clearTimeout"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4 days.
let gNextTimeoutId = 1; // setTimeout must return a positive integer

let gTimeoutTable = new Map(); // int -> nsITimer

this.setTimeout = function setTimeout(aCallback, aMilliseconds) {
  let id = gNextTimeoutId++;
  let args = Array.slice(arguments, 2);
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(function setTimeout_timer() {
    gTimeoutTable.delete(id);
    aCallback.apply(null, args);
  }, aMilliseconds, timer.TYPE_ONE_SHOT);

  gTimeoutTable.set(id, timer);
  return id;
}

this.clearTimeout = function clearTimeout(aId) {
  if (gTimeoutTable.has(aId)) {
    gTimeoutTable.get(aId).cancel();
    gTimeoutTable.delete(aId);
  }
}
