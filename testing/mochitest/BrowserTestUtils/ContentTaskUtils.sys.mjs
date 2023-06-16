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

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

export var ContentTaskUtils = {
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
    if (
      element.parentNode != element.ownerDocument &&
      element.parentNode.nodeType != Node.DOCUMENT_FRAGMENT_NODE
    ) {
      return ContentTaskUtils.is_hidden(element.parentNode);
    }

    // Walk up the shadow DOM if we've reached the top of the shadow root
    if (element.parentNode.host) {
      return ContentTaskUtils.is_hidden(element.parentNode.host);
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
    return !this.is_hidden(element);
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
  async waitForCondition(condition, msg, interval = 100, maxTries = 50) {
    let startTime = Cu.now();
    for (let tries = 0; tries < maxTries; ++tries) {
      await new Promise(resolve => setTimeout(resolve, interval));

      let conditionPassed = false;
      try {
        conditionPassed = await condition();
      } catch (e) {
        msg += ` - threw exception: ${e}`;
        ChromeUtils.addProfilerMarker(
          "ContentTaskUtils",
          { startTime, category: "Test" },
          `waitForCondition - ${msg}`
        );
        throw msg;
      }
      if (conditionPassed) {
        ChromeUtils.addProfilerMarker(
          "ContentTaskUtils",
          { startTime, category: "Test" },
          `waitForCondition succeeded after ${tries} retries - ${msg}`
        );
        return conditionPassed;
      }
    }

    msg += ` - timed out after ${maxTries} tries.`;
    ChromeUtils.addProfilerMarker(
      "ContentTaskUtils",
      { startTime, category: "Test" },
      `waitForCondition - ${msg}`
    );
    throw msg;
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
      let startTime = Cu.now();
      subject.addEventListener(
        eventName,
        function listener(event) {
          try {
            if (checkFn && !checkFn(event)) {
              return;
            }
            subject.removeEventListener(eventName, listener, capture);
            setTimeout(() => {
              ChromeUtils.addProfilerMarker(
                "ContentTaskUtils",
                { category: "Test", startTime },
                "waitForEvent - " + eventName
              );
              resolve(event);
            }, 0);
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
   * Wait until DOM mutations cause the condition expressed in checkFn to pass.
   * Intended as an easy-to-use alternative to waitForCondition.
   *
   * @param {Element} subject
   *        The element on which to observe mutations.
   * @param {Object} options
   *        The options to pass to MutationObserver.observe();
   * @param {function} checkFn [optional]
   *        Function that returns true when it wants the promise to be resolved.
   *        If not specified, the first mutation will resolve the promise.
   *
   * @returns {Promise<void>}
   */
  waitForMutationCondition(subject, options, checkFn) {
    if (checkFn?.()) {
      return Promise.resolve();
    }
    return new Promise(resolve => {
      let obs = new subject.ownerGlobal.MutationObserver(function () {
        if (checkFn && !checkFn()) {
          return;
        }
        obs.disconnect();
        resolve();
      });
      obs.observe(subject, options);
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
    EventUtils.setTimeout = setTimeout;
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
