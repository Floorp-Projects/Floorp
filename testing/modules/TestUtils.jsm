/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Contains a limited number of testing functions that are commonly used in a
 * wide variety of situations, for example waiting for an event loop tick or an
 * observer notification.
 *
 * More complex functions are likely to belong to a separate test-only module.
 * Examples include Assert.jsm for generic assertions, FileTestUtils.jsm to work
 * with local files and their contents, and BrowserTestUtils.jsm to work with
 * browser windows and tabs.
 *
 * Individual components also offer testing functions to other components, for
 * example LoginTestUtils.jsm.
 */

"use strict";

var EXPORTED_SYMBOLS = ["TestUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var TestUtils = {
  executeSoon(callbackFn) {
    Services.tm.dispatchToMainThread(callbackFn);
  },

  waitForTick() {
    return new Promise(resolve => this.executeSoon(resolve));
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
   * Waits for the specified preference to be change.
   *
   * @param {string} prefName
   *        The pref to observe.
   * @param {function} checkFn [optional]
   *        Called with the new preference value as argument, should return true if the
   *        notification is the expected one, or false if it should be ignored
   *        and listening should continue. If not specified, the first
   *        notification for the specified topic resolves the returned promise.
   *
   * @note Because this function is intended for testing, any error in checkFn
   *       will cause the returned promise to be rejected instead of waiting for
   *       the next notification, since this is probably a bug in the test.
   *
   * @return {Promise}
   * @resolves The value of the preference.
   */
  waitForPrefChange(prefName, checkFn) {
    return new Promise((resolve, reject) => {
      Services.prefs.addObserver(prefName, function observer(
        subject,
        topic,
        data
      ) {
        try {
          let prefValue = null;
          switch (Services.prefs.getPrefType(prefName)) {
            case Services.prefs.PREF_STRING:
              prefValue = Services.prefs.getStringPref(prefName);
              break;
            case Services.prefs.PREF_INT:
              prefValue = Services.prefs.getIntPref(prefName);
              break;
            case Services.prefs.PREF_BOOL:
              prefValue = Services.prefs.getBoolPref(prefName);
              break;
          }
          if (checkFn && !checkFn(prefValue)) {
            return;
          }
          Services.prefs.removeObserver(prefName, observer);
          resolve(prefValue);
        } catch (ex) {
          Services.prefs.removeObserver(prefName, observer);
          reject(ex);
        }
      });
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
    if (Element.isInstance(eltOrRect)) {
      eltOrRect = eltOrRect.getBoundingClientRect();
    }
    let { left, top, width, height } = eltOrRect;
    let canvas = win.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas"
    );
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
   *        function can be an async function.
   * @param interval
   *        The time interval to poll the condition function. Defaults
   *        to 100ms.
   * @param attempts
   *        The number of times to poll before giving up and rejecting
   *        if the condition has not yet returned true. Defaults to 50
   *        (~5 seconds for 100ms intervals)
   * @return Promise
   *        Resolves with the return value of the condition function.
   *        Rejects if timeout is exceeded or condition ever throws.
   *
   * NOTE: This is intentionally not using setInterval, using setTimeout
   * instead. setInterval is not promise-safe.
   */
  waitForCondition(condition, msg, interval = 100, maxTries = 50) {
    let startTime = Cu.now();
    return new Promise((resolve, reject) => {
      let tries = 0;
      async function tryOnce() {
        if (tries >= maxTries) {
          msg += ` - timed out after ${maxTries} tries.`;
          reject(msg);
          return;
        }

        let conditionPassed = false;
        try {
          conditionPassed = await condition();
        } catch (e) {
          msg += ` - threw exception: ${e}`;
          reject(msg);
          return;
        }

        if (conditionPassed) {
          ChromeUtils.addProfilerMarker(
            "TestUtils",
            { startTime, category: "Test" },
            `waitForCondition succeeded after ${tries} retries - ${msg}`
          );
          resolve(conditionPassed);
          return;
        }
        tries++;
        setTimeout(tryOnce, interval);
      }

      // FIXME(bug 1596165): This could be a direct call, ideally.
      setTimeout(tryOnce, interval);
    });
  },

  shuffle(array) {
    let results = [];
    for (let i = 0; i < array.length; ++i) {
      let randomIndex = Math.floor(Math.random() * (i + 1));
      results[i] = results[randomIndex];
      results[randomIndex] = array[i];
    }
    return results;
  },
};
