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

// Disable ownerGlobal use since that's not available on content-privileged elements.

/* eslint-disable mozilla/use-ownerGlobal */

"use strict";

var EXPORTED_SYMBOLS = ["ContentTaskUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { clearInterval, setInterval, setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

var ContentTaskUtils = {
  /**
   * Checks if a DOM element is hidden.
   *
   * @param {Element} element
   *        The element which is to be checked.
   *
   * @return {boolean}
   */
  is_hidden(element) {
    let style = element.ownerDocument.defaultView.getComputedStyle(element);
    if (style.display == "none") {
      return true;
    }
    if (style.visibility != "visible") {
      return true;
    }

    // Hiding a parent element will hide all its children
    if (element.parentNode != element.ownerDocument) {
      return ContentTaskUtils.is_hidden(element.parentNode);
    }

    return false;
  },

  /**
   * Checks if a DOM element is visible.
   *
   * @param {Element} element
   *        The element which is to be checked.
   *
   * @return {boolean}
   */
  is_visible(element) {
    let style = element.ownerDocument.defaultView.getComputedStyle(element);
    if (style.display == "none") {
      return false;
    }
    if (style.visibility != "visible") {
      return false;
    }

    // Hiding a parent element will hide all its children
    if (element.parentNode != element.ownerDocument) {
      return ContentTaskUtils.is_visible(element.parentNode);
    }

    return true;
  },

  /**
   * Will poll a condition function until it returns true.
   *
   * @param condition
   *        A condition function that must return true or false. If the
   *        condition ever throws, this is also treated as a false.
   * @param msg
   *        The message to use when the returned promise is rejected.
   *        This message will be extended with additional information
   *        about the number of tries or the thrown exception.
   * @param interval
   *        The time interval to poll the condition function. Defaults
   *        to 100ms.
   * @param maxTries
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
        } catch (e) {
          msg += ` - threw exception: ${e}`;
          clearInterval(intervalID);
          reject(msg);
          return;
        }

        if (conditionPassed) {
          clearInterval(intervalID);
          resolve(conditionPassed);
        }
        tries++;
      }, interval);
    });
  },

  /**
   * Waits for an event to be fired on a specified element.
   *
   * Usage:
   *    let promiseEvent = ContentTasKUtils.waitForEvent(element, "eventName");
   *    // Do some processing here that will cause the event to be fired
   *    // ...
   *    // Now yield until the Promise is fulfilled
   *    let receivedEvent = yield promiseEvent;
   *
   * @param {Element} subject
   *        The element that should receive the event.
   * @param {string} eventName
   *        Name of the event to listen to.
   * @param {bool} capture [optional]
   *        True to use a capturing listener.
   * @param {function} checkFn [optional]
   *        Called with the Event object as argument, should return true if the
   *        event is the expected one, or false if it should be ignored and
   *        listening should continue. If not specified, the first event with
   *        the specified name resolves the returned promise.
   *
   * @note Because this function is intended for testing, any error in checkFn
   *       will cause the returned promise to be rejected instead of waiting for
   *       the next event, since this is probably a bug in the test.
   *
   * @returns {Promise}
   * @resolves The Event object.
   */
  waitForEvent(subject, eventName, capture, checkFn, wantsUntrusted = false) {
    return new Promise((resolve, reject) => {
      subject.addEventListener(
        eventName,
        function listener(event) {
          try {
            if (checkFn && !checkFn(event)) {
              return;
            }
            subject.removeEventListener(eventName, listener, capture);
            setTimeout(() => resolve(event), 0);
          } catch (ex) {
            try {
              subject.removeEventListener(eventName, listener, capture);
            } catch (ex2) {
              // Maybe the provided object does not support removeEventListener.
            }
            setTimeout(() => reject(ex), 0);
          }
        },
        capture,
        wantsUntrusted
      );
    });
  },

  /**
   * Gets an instance of the `EventUtils` helper module for usage in
   * content tasks. See https://searchfox.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/EventUtils.js
   *
   * @param content
   *        The `content` global object from your content task.
   *
   * @returns an EventUtils instance.
   */
  getEventUtils(content) {
    if (content._EventUtils) {
      return content._EventUtils;
    }

    let EventUtils = (content._EventUtils = {});

    EventUtils.window = {};
    EventUtils.parent = EventUtils.window;
    /* eslint-disable camelcase */
    EventUtils._EU_Ci = Ci;
    EventUtils._EU_Cc = Cc;
    /* eslint-enable camelcase */
    // EventUtils' `sendChar` function relies on the navigator to synthetize events.
    EventUtils.navigator = content.navigator;
    EventUtils.KeyboardEvent = content.KeyboardEvent;

    Services.scriptloader.loadSubScript(
      "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
      EventUtils
    );

    return EventUtils;
  },
};
