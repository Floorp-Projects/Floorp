/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * JS module implementation of setTimeout and clearTimeout.
 */

this.EXPORTED_SYMBOLS = ["setTimeout", "setTimeoutWithTarget", "clearTimeout",
                         "setInterval", "setIntervalWithTarget", "clearInterval"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4 days.
var gNextId = 1; // setTimeout and setInterval must return a positive integer

var gTimerTable = new Map(); // int -> nsITimer

function _setTimeoutOrIsInterval(aCallback, aMilliseconds, aIsInterval,
                                 aTarget, aArgs) {
  let id = gNextId++;
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  if (aTarget) {
    timer.target = aTarget;
  }

  let callback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback,
                                           Ci.nsINamed]),

    // nsITimerCallback
    notify() {
      if (!aIsInterval) {
        gTimerTable.delete(id);
      }
      aCallback.apply(null, aArgs);
    },

    // nsINamed
    name: (aIsInterval ? "setInterval() for " : "setTimeout() for ") +
            Cu.generateXPCWrappedJS(aCallback).QueryInterface(Ci.nsINamed).name,
  };

  timer.initWithCallback(callback, aMilliseconds,
    aIsInterval ? timer.TYPE_REPEATING_SLACK : timer.TYPE_ONE_SHOT);

  gTimerTable.set(id, timer);
  return id;
}

this.setTimeout = function setTimeout(aCallback, aMilliseconds, ...aArgs) {
  return _setTimeoutOrIsInterval(
    aCallback, aMilliseconds, false, null, aArgs);
};

this.setTimeoutWithTarget = function setTimeoutWithTarget(aCallback,
                                                          aMilliseconds,
                                                          aTarget,
                                                          ...aArgs) {
  return _setTimeoutOrIsInterval(
    aCallback, aMilliseconds, false, aTarget, aArgs);
};

this.setInterval = function setInterval(aCallback, aMilliseconds, ...aArgs) {
  return _setTimeoutOrIsInterval(
    aCallback, aMilliseconds, true, null, aArgs);
};

this.setIntervalWithTarget = function setIntervalWithTarget(aCallback,
                                                            aMilliseconds,
                                                            aTarget,
                                                            ...aArgs) {
  return _setTimeoutOrIsInterval(
    aCallback, aMilliseconds, true, aTarget, aArgs);
};

this.clearInterval = this.clearTimeout = function clearTimeout(aId) {
  if (gTimerTable.has(aId)) {
    gTimerTable.get(aId).cancel();
    gTimerTable.delete(aId);
  }
};
