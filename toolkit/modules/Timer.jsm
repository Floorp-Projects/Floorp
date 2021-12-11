/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * JS module implementation of setTimeout and clearTimeout.
 */

var EXPORTED_SYMBOLS = [
  "setTimeout",
  "setTimeoutWithTarget",
  "clearTimeout",
  "setInterval",
  "setIntervalWithTarget",
  "clearInterval",
  "requestIdleCallback",
  "cancelIdleCallback",
];

// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4 days.
var gNextId = 1; // setTimeout and setInterval must return a positive integer

var gTimerTable = new Map(); // int -> nsITimer or idleCallback

// Don't generate this for every timer.
var setTimeout_timerCallbackQI = ChromeUtils.generateQI([
  "nsITimerCallback",
  "nsINamed",
]);

function _setTimeoutOrIsInterval(
  aCallback,
  aMilliseconds,
  aIsInterval,
  aTarget,
  aArgs
) {
  if (typeof aCallback !== "function") {
    throw new Error(
      `callback is not a function in ${
        aIsInterval ? "setInterval" : "setTimeout"
      }`
    );
  }
  let id = gNextId++;
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  if (aTarget) {
    timer.target = aTarget;
  }

  let callback = {
    QueryInterface: setTimeout_timerCallbackQI,

    // nsITimerCallback
    notify() {
      if (!aIsInterval) {
        gTimerTable.delete(id);
      }
      aCallback.apply(null, aArgs);
    },

    // nsINamed
    name:
      (aIsInterval ? "setInterval() for " : "setTimeout() for ") +
      Cu.generateXPCWrappedJS(aCallback).QueryInterface(Ci.nsINamed).name,
  };

  timer.initWithCallback(
    callback,
    aMilliseconds,
    aIsInterval ? timer.TYPE_REPEATING_SLACK : timer.TYPE_ONE_SHOT
  );

  gTimerTable.set(id, timer);
  return id;
}

function setTimeout(aCallback, aMilliseconds, ...aArgs) {
  return _setTimeoutOrIsInterval(aCallback, aMilliseconds, false, null, aArgs);
}

function setTimeoutWithTarget(aCallback, aMilliseconds, aTarget, ...aArgs) {
  return _setTimeoutOrIsInterval(
    aCallback,
    aMilliseconds,
    false,
    aTarget,
    aArgs
  );
}

function setInterval(aCallback, aMilliseconds, ...aArgs) {
  return _setTimeoutOrIsInterval(aCallback, aMilliseconds, true, null, aArgs);
}

function setIntervalWithTarget(aCallback, aMilliseconds, aTarget, ...aArgs) {
  return _setTimeoutOrIsInterval(
    aCallback,
    aMilliseconds,
    true,
    aTarget,
    aArgs
  );
}

function clear(aId) {
  if (gTimerTable.has(aId)) {
    gTimerTable.get(aId).cancel();
    gTimerTable.delete(aId);
  }
}
var clearInterval = clear;
var clearTimeout = clear;

function requestIdleCallback(aCallback, aOptions) {
  if (typeof aCallback !== "function") {
    throw new Error("callback is not a function in requestIdleCallback");
  }
  let id = gNextId++;

  let callback = (...aArgs) => {
    if (gTimerTable.has(id)) {
      gTimerTable.delete(id);
      aCallback(...aArgs);
    }
  };

  ChromeUtils.idleDispatch(callback, aOptions);
  gTimerTable.set(id, callback);
  return id;
}

function cancelIdleCallback(aId) {
  if (gTimerTable.has(aId)) {
    gTimerTable.delete(aId);
  }
}
