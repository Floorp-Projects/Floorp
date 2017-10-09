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
Cu.import("resource://gre/modules/Timer.jsm");

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

  /**
   * Takes a screenshot of an area and returns it as a data URL.
   *
   * @param eltOrRect
   *        The DOM node or rect ({left, top, width, height}) to screenshot.
   * @param win
   *        The current window.
   */
  screenshotArea(eltOrRect, win) {
    if (eltOrRect instanceof Ci.nsIDOMElement) {
      eltOrRect = eltOrRect.getBoundingClientRect();
    }
    let { left, top, width, height } = eltOrRect;
    let canvas = win.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    let ctx = canvas.getContext("2d");
    let ratio = win.devicePixelRatio;
    canvas.width = width * ratio;
    canvas.height = height * ratio;
    ctx.scale(ratio, ratio);
    ctx.drawWindow(win, left, top, width, height, "#fff");
    return canvas.toDataURL();
  },

    /**
   * Will poll a condition function until it returns true.
   *
   * @param condition
   *        A condition function that must return true or false. If the
   *        condition ever throws, this is also treated as a false. The
   *        function can be a generator.
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
  waitForCondition(condition, msg, interval = 100, maxTries = 50) {
    return new Promise((resolve, reject) => {
      let tries = 0;
      let intervalID = setInterval(async function() {
        if (tries >= maxTries) {
          clearInterval(intervalID);
          msg += ` - timed out after ${maxTries} tries.`;
          reject(msg);
          return;
        }

        let conditionPassed = false;
        try {
          conditionPassed = await condition();
        } catch (e) {
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
  },
};
