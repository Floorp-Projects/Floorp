/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "DOMContentLoadedPromise",
  "EventPromise",
  "MessagePromise",
];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
function EventPromise(listener, type, options = {
    capture: false,
    wantsUntrusted: false,
    mozSystemGroup: false,
  }) {
  if (!listener || !("addEventListener" in listener)) {
    throw new TypeError();
  }
  if (typeof type != "string") {
    throw new TypeError();
  }
  if (("capture" in options && typeof options.capture != "boolean") ||
      ("wantsUntrusted" in options && typeof options.wantsUntrusted != "boolean") ||
      ("mozSystemGroup" in options && typeof options.mozSystemGroup != "boolean")) {
    throw new TypeError();
  }

  options.once = true;

  return new Promise(resolve => {
    listener.addEventListener(type, event => {
      Services.tm.dispatchToMainThread(() => resolve(event));
    }, options);
  });
}

function DOMContentLoadedPromise(window, options = {mozSystemGroup: true}) {
  if (window.document.readyState == "complete" || window.document.readyState == "interactive") {
    return Promise.resolve();
  }
  return new EventPromise(window, "DOMContentLoaded", options);
}

/**
 * Awaits a single IPC message.
 *
 * @param {nsIMessageSender} target
 * @param {string} name
 *
 * @return {Promise}
 *
 * @throws {TypeError}
 *     If target is not an nsIMessageSender.
 */
function MessagePromise(target, name) {
  if (!(target instanceof Ci.nsIMessageSender)) {
    throw new TypeError();
  }

  return new Promise(resolve => {
    const onMessage = (...args) => {
      target.removeMessageListener(name, onMessage);
      resolve(...args);
    };
    target.addMessageListener(name, onMessage);
  });
}
