/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module implements a number of utilities useful for testing.
 *
 * Additions to this module should be generic and useful to testing multiple
 * features. Utilities only useful to a sepcific testing framework should live
 * in a module specific to that framework, such as BrowserTestUtils.jsm in the
 * case of mochitest-browser-chrome.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.TestUtils = {
  executeSoon(callbackFn) {
    Services.tm.dispatchToMainThread(callbackFn);
  },

  /**
   * Waits for the specified topic to be observed.
   *
   * @param {string} topic
   *        The topic to observe.
   * @param {function} checkFn [optional]
   *        Called with (subject, data) as arguments, should return true if the
   *        notification is the expected one, or false if it should be ignored
   *        and listening should continue. If not specified, the first
   *        notification for the specified topic resolves the returned promise.
   *
   * @note Because this function is intended for testing, any error in checkFn
   *       will cause the returned promise to be rejected instead of waiting for
   *       the next notification, since this is probably a bug in the test.
   *
   * @return {Promise}
   * @resolves The array [subject, data] from the observed notification.
   */
  topicObserved(topic, checkFn) {
    return new Promise((resolve, reject) => {
      Services.obs.addObserver(function observer(subject, topic, data) {
        try {
          if (checkFn && !checkFn(subject, data)) {
            return;
          }
          Services.obs.removeObserver(observer, topic);
          resolve([subject, data]);
        } catch (ex) {
          Services.obs.removeObserver(observer, topic);
          reject(ex);
        }
      }, topic);
    });
  },
};
