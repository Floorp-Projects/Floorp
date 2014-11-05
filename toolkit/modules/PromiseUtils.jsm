/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

this.EXPORTED_SYMBOLS = ["PromiseUtils"];

Components.utils.import("resource://gre/modules/Timer.jsm");

this.PromiseUtils = {
  /*
   * A simple timeout mechanism.
   *
   * Example:
   * resolveOrTimeout(myModule.shutdown(), 1000, new Error("The module took too long to shutdown"));
   *
   * @param {Promise} promise The Promise that should resolve/reject quickly.
   * @param {number} delay A delay after which to stop waiting for `promise`, in milliseconds.
   * @param {function} rejection If `promise` hasn't resolved/rejected after `delay`,
   * a value used to construct the rejection.
   *
   * @return {Promise} A promise that behaves as `promise`, if `promise` is
   * resolved/rejected within `delay` ms, or rejects with `rejection()` otherwise.
   */
  resolveOrTimeout : function(promise, delay, rejection)  {
    // throw a TypeError if <promise> is not a Promise object
    if (!(promise instanceof Promise)) {
      throw new TypeError("first argument <promise> must be a Promise object");
    }

    // throw a TypeError if <delay> is not a number
    if (typeof delay != "number" || delay < 0) {
      throw new TypeError("second argument <delay> must be a positive number");
    }

    // throws a TypeError if <rejection> is not a function
    if (rejection && typeof rejection != "function") {
      throw new TypeError("third optional argument <rejection> must be a function");
    }

    return new Promise((resolve, reject) => {
      promise.then(resolve, reject);
      let id = setTimeout(() => {
        try {
          rejection ? reject(rejection()) : reject(new Error("Promise Timeout"));
        } catch(ex) {
          reject(ex);
        }
        clearTimeout(id);
      }, delay);
    });
  }
}