/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export var PromiseUtils = {
  /**
   * Requests idle dispatch to the main thread for the given callback,
   * and returns a promise which resolves to the callback's return value
   * when it has been executed.
   *
   * @param {function} callback
   * @param {integer} [timeout]
   *        An optional timeout, after which the callback will be
   *        executed immediately if idle dispatch has not yet occurred.
   *
   * @returns {Promise}
   */
  idleDispatch(callback, timeout = 0) {
    return new Promise((resolve, reject) => {
      Services.tm.idleDispatchToMainThread(() => {
        try {
          resolve(callback());
        } catch (e) {
          reject(e);
        }
      }, timeout);
    });
  },
};
