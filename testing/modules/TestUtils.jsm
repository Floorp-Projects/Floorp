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
    Services.tm.mainThread.dispatch(callbackFn, Ci.nsIThread.DISPATCH_NORMAL);
  },

  /**
   * Waits for the specified topic to be observed.
   *
   * @param {string} topic
   *        The topic to observe.
   * @param {*} subject
   *        A value to check the notification subject against. Only a
   *        notification with a matching subject will cause the promise to
   *        resolve.
   * @return {Promise}
   *         Resolves with the data provided when the topic has been observed.
   */
  topicObserved(topic, subject=null) {
    return new Promise(resolve => {
      Services.obs.addObserver(function observer(observedSubject, topic, data) {
        if (subject !== null && subject !== observedSubject) { return; }

        Services.obs.removeObserver(observer, topic);
        resolve(data);
      }, topic, false);
    });
  },
};
