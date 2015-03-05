/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module implements a number of utilities useful for browser tests.
 *
 * All asynchronous helper methods should return promises, rather than being
 * callback based.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "BrowserTestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");

Cc["@mozilla.org/globalmessagemanager;1"]
  .getService(Ci.nsIMessageListenerManager)
  .loadFrameScript(
    "chrome://mochikit/content/tests/BrowserTestUtils/content-utils.js", true);


/**
 * Default wait period in millseconds, when waiting for the expected
 * event to occur.
 * @type {number}
 */
const DEFAULT_WAIT = 2000;


this.BrowserTestUtils = {
  /**
   * @param {xul:browser} browser
   *        A xul:browser.
   * @param {Boolean} includeSubFrames
   *        A boolean indicating if loads from subframes should be included.
   * @return {Promise}
   *         A Promise which resolves when a load event is triggered
   *         for browser.
   */
  browserLoaded(browser, includeSubFrames=false) {
    return new Promise(resolve => {
      browser.messageManager.addMessageListener("browser-test-utils:loadEvent",
                                                 function onLoad(msg) {
        if (!msg.data.subframe || includeSubFrames) {
          browser.messageManager.removeMessageListener(
            "browser-test-utils:loadEvent", onLoad);
          resolve();
        }
      });
    });
  },

  /**
   * @return {Promise}
   *         A Promise which resolves when a "domwindowopened" notification
   *         has been fired by the window watcher.
   */
  domWindowOpened() {
    return new Promise(resolve => {
      function observer(subject, topic, data) {
        if (topic != "domwindowopened") { return; }

        Services.ww.unregisterNotification(observer);
        resolve(subject.QueryInterface(Ci.nsIDOMWindow));
      }
      Services.ww.registerNotification(observer);
    });
  },

  /**
   * @param {Object} options
   *        {
   *          private: A boolean indicating if the window should be
   *                   private
   *        }
   * @return {Promise}
   *         Resolves with the new window once it is loaded.
   */
  openNewBrowserWindow(options) {
    let argString = Cc["@mozilla.org/supports-string;1"].
                    createInstance(Ci.nsISupportsString);
    argString.data = "";
    let features = "chrome,dialog=no,all";

    if (options && options.private || false) {
      features += ",private";
    }

    let win = Services.ww.openWindow(
      null, Services.prefs.getCharPref("browser.chromeURL"), "_blank",
      features, argString);

    // Wait for browser-delayed-startup-finished notification, it indicates
    // that the window has loaded completely and is ready to be used for
    // testing.
    return TestUtils.topicObserved("browser-delayed-startup-finished",
                                   subject => subject == win).then(() => win);
  },

  /**
   * Closes a window.
   *
   * @param {Window}
   *        A window to close.
   *
   * @return {Promise}
   *         Resolves when the provided window has been closed.
   */
  closeWindow(win) {
    return new Promise(resolve => {
      function observer(subject, topic, data) {
        if (topic == "domwindowclosed" && subject === win) {
          Services.ww.unregisterNotification(observer);
          resolve();
        }
      }
      Services.ww.registerNotification(observer);
      win.close();
    });
  },


  /**
   * Waits a specified number of miliseconds for a specified event to be
   * fired on a specified element.
   *
   * Usage:
   *    let receivedEvent = BrowserTestUtil.waitForEvent(element, "eventName");
   *    // Do some processing here that will cause the event to be fired
   *    // ...
   *    // Now yield until the Promise is fulfilled
   *    yield receivedEvent;
   *    if (receivedEvent && !(receivedEvent instanceof Error)) {
   *      receivedEvent.msg == "eventName";
   *      // ...
   *    }
   *
   * @param {Element} subject - The element that should receive the event.
   * @param {string} eventName - The event to wait for.
   * @param {number} timeoutMs - The number of miliseconds to wait before giving up.
   * @param {Element} target - Expected target of the event.
   * @returns {Promise} A Promise that resolves to the received event, or
   *                    rejects with an Error.
   */
  waitForEvent(subject, eventName, timeoutMs, target) {
    return new Promise((resolve, reject) => {
      function listener(event) {
        if (target && target !== event.target) {
          return;
        }

        subject.removeEventListener(eventName, listener);
        clearTimeout(timerID);
        resolve(event);
      }

      timeoutMs = timeoutMs || DEFAULT_WAIT;
      let stack = new Error().stack;

      let timerID = setTimeout(() => {
        subject.removeEventListener(eventName, listener);
        reject(new Error(`${eventName} event timeout at ${stack}`));
      }, timeoutMs);


      subject.addEventListener(eventName, listener);
    });
  },
};
