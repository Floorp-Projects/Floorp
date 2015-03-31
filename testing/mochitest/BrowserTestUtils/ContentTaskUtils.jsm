/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module implements a number of utility functions that can be loaded
 * into content scope.
 *
 * All asynchronous helper methods should return promises, rather than being
 * callback based.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "ContentTaskUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Timer.jsm");

this.ContentTaskUtils = {
  /**
   * Will poll a condition function until it returns true.
   *
   * @param condition
   *        A condition function that must return true or false. If the
   *        condition ever throws, this is also treated as a false.
   * @param interval
   *        The time interval to poll the condition function. Defaults
   *        to 100ms.
   * @param attempts
   *        The number of times to poll before giving up and rejecting
   *        if the condition has not yet returned true. Defaults to 50
   *        (~5 seconds for 100ms intervals)
   * @return Promise
   *        Resolves when condition is true.
   *        Rejects if timeout is exceeded or condition ever throws.
   */
  waitForCondition(condition, msg, interval=100, maxTries=50) {
    return new Promise((resolve, reject) => {
      let tries = 0;
      let intervalID = setInterval(() => {
        if (tries >= maxTries) {
          clearInterval(intervalID);
          msg += ` - timed out after ${maxTries} tries.`;
          reject(msg);
          return;
        }

        let conditionPassed = false;
        try {
          conditionPassed = condition();
        } catch(e) {
          msg += ` - threw exception: ${e}`;
          clearInterval(intervalID);
          reject(msg);
          return;
        }

        if (conditionPassed) {
          clearInterval(intervalID);
          resolve();
        }
        tries++;
      }, interval);
    });
  }
};
