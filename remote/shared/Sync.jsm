/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "AnimationFramePromise",
  "EventPromise",
  "executeSoon",
  "PollPromise",
];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

const { TYPE_REPEATING_SLACK } = Ci.nsITimer;

/**
 * Wait for a single event to be fired on a specific EventListener.
 *
 * The returned promise is guaranteed to not be called before the
 * next event tick after the event listener is called, so that all
 * other event listeners for the element are executed before the
 * handler is executed.  For example:
 *
 *     const promise = new EventPromise(element, "myEvent");
 *     // same event tick here
 *     await promise;
 *     // next event tick here
 *
 * @param {EventListener} listener
 *     Object which receives a notification (an object that implements
 *     the Event interface) when an event of the specificed type occurs.
 * @param {string} type
 *     Case-sensitive string representing the event type to listen for.
 * @param {boolean?} [false] options.capture
 *     Indicates the event will be despatched to this subject,
 *     before it bubbles down to any EventTarget beneath it in the
 *     DOM tree.
 * @param {boolean?} [false] options.wantsUntrusted
 *     Receive synthetic events despatched by web content.
 * @param {boolean?} [false] options.mozSystemGroup
 *     Determines whether to add listener to the system group.
 *
 * @return {Promise.<Event>}
 *
 * @throws {TypeError}
 */
function EventPromise(
  listener,
  type,
  options = {
    capture: false,
    wantsUntrusted: false,
    mozSystemGroup: false,
  }
) {
  if (!listener || !("addEventListener" in listener)) {
    throw new TypeError();
  }
  if (typeof type != "string") {
    throw new TypeError();
  }
  if (
    ("capture" in options && typeof options.capture != "boolean") ||
    ("wantsUntrusted" in options &&
      typeof options.wantsUntrusted != "boolean") ||
    ("mozSystemGroup" in options && typeof options.mozSystemGroup != "boolean")
  ) {
    throw new TypeError();
  }

  options.once = true;

  return new Promise(resolve => {
    listener.addEventListener(
      type,
      event => {
        executeSoon(() => resolve(event));
      },
      options
    );
  });
}

/**
 * Wait for the next tick in the event loop to execute a callback.
 *
 * @param {function} fn
 *     Function to be executed.
 */
function executeSoon(fn) {
  if (typeof fn != "function") {
    throw new TypeError();
  }

  Services.tm.dispatchToMainThread(fn);
}

/**
 * Throttle until the `window` has performed an animation frame.
 *
 * @param {ChromeWindow} win
 *     Window to request the animation frame from.
 *
 * @return {Promise
 */
function AnimationFramePromise(win) {
  const animationFramePromise = new Promise(resolve => {
    win.requestAnimationFrame(resolve);
  });

  // Abort if the underlying window gets closed
  const windowClosedPromise = new PollPromise(resolve => {
    if (win.closed) {
      resolve();
    }
  });

  return Promise.race([animationFramePromise, windowClosedPromise]);
}

/**
 * Runs a Promise-like function off the main thread until it is resolved
 * through ``resolve`` or ``rejected`` callbacks.  The function is
 * guaranteed to be run at least once, irregardless of the timeout.
 *
 * The ``func`` is evaluated every ``interval`` for as long as its
 * runtime duration does not exceed ``interval``.  Evaluations occur
 * sequentially, meaning that evaluations of ``func`` are queued if
 * the runtime evaluation duration of ``func`` is greater than ``interval``.
 *
 * ``func`` is given two arguments, ``resolve`` and ``reject``,
 * of which one must be called for the evaluation to complete.
 * Calling ``resolve`` with an argument indicates that the expected
 * wait condition was met and will return the passed value to the
 * caller.  Conversely, calling ``reject`` will evaluate ``func``
 * again until the ``timeout`` duration has elapsed or ``func`` throws.
 * The passed value to ``reject`` will also be returned to the caller
 * once the wait has expired.
 *
 * Usage::
 *
 *     let els = new PollPromise((resolve, reject) => {
 *       let res = document.querySelectorAll("p");
 *       if (res.length > 0) {
 *         resolve(Array.from(res));
 *       } else {
 *         reject([]);
 *       }
 *     }, {timeout: 1000});
 *
 * @param {Condition} func
 *     Function to run off the main thread.
 * @param {number=} [timeout] timeout
 *     Desired timeout if wanted.  If 0 or less than the runtime evaluation
 *     time of ``func``, ``func`` is guaranteed to run at least once.
 *     Defaults to using no timeout.
 * @param {number=} [interval=10] interval
 *     Duration between each poll of ``func`` in milliseconds.
 *     Defaults to 10 milliseconds.
 *
 * @return {Promise.<*>}
 *     Yields the value passed to ``func``'s
 *     ``resolve`` or ``reject`` callbacks.
 *
 * @throws {*}
 *     If ``func`` throws, its error is propagated.
 * @throws {TypeError}
 *     If `timeout` or `interval`` are not numbers.
 * @throws {RangeError}
 *     If `timeout` or `interval` are not unsigned integers.
 */
function PollPromise(func, { timeout = null, interval = 10 } = {}) {
  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  if (typeof func != "function") {
    throw new TypeError();
  }
  if (timeout != null && typeof timeout != "number") {
    throw new TypeError();
  }
  if (typeof interval != "number") {
    throw new TypeError();
  }
  if (
    (timeout && (!Number.isInteger(timeout) || timeout < 0)) ||
    !Number.isInteger(interval) ||
    interval < 0
  ) {
    throw new RangeError();
  }

  return new Promise((resolve, reject) => {
    let start, end;

    if (Number.isInteger(timeout)) {
      start = new Date().getTime();
      end = start + timeout;
    }

    let evalFn = () => {
      new Promise(func)
        .then(resolve, rejected => {
          if (typeof rejected != "undefined") {
            throw rejected;
          }

          // return if there is a timeout and set to 0,
          // allowing |func| to be evaluated at least once
          if (
            typeof end != "undefined" &&
            (start == end || new Date().getTime() >= end)
          ) {
            resolve(rejected);
          }
        })
        .catch(reject);
    };

    // the repeating slack timer waits |interval|
    // before invoking |evalFn|
    evalFn();

    timer.init(evalFn, interval, TYPE_REPEATING_SLACK);
  }).then(
    res => {
      timer.cancel();
      return res;
    },
    err => {
      timer.cancel();
      throw err;
    }
  );
}
