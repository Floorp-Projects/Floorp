/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * JS module implementation of setTimeout and clearTimeout.
 */

this.EXPORTED_SYMBOLS = ["setTimeout", "clearTimeout", "setInterval", "clearInterval"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4 days.
var gNextId = 1; // setTimeout and setInterval must return a positive integer

var gTimerTable = new Map(); // int -> nsITimer

this.setTimeout = function setTimeout(aCallback, aMilliseconds) {
  let id = gNextId++;
  let args = Array.slice(arguments, 2);
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(function setTimeout_timer() {
    gTimerTable.delete(id);
    aCallback.apply(null, args);
  }, aMilliseconds, timer.TYPE_ONE_SHOT);

  gTimerTable.set(id, timer);
  return id;
}

this.setInterval = function setInterval(aCallback, aMilliseconds) {
  let id = gNextId++;
  let args = Array.slice(arguments, 2);
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(function setInterval_timer() {
    aCallback.apply(null, args);
  }, aMilliseconds, timer.TYPE_REPEATING_SLACK);

  gTimerTable.set(id, timer);
  return id;
}

this.clearInterval = this.clearTimeout = function clearTimeout(aId) {
  if (gTimerTable.has(aId)) {
    gTimerTable.get(aId).cancel();
    gTimerTable.delete(aId);
  }
}
