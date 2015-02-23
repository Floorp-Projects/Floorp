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
Cu.import("resource://testing-common/TestUtils.jsm");

Cc["@mozilla.org/globalmessagemanager;1"]
  .getService(Ci.nsIMessageListenerManager)
  .loadFrameScript(
    "chrome://mochikit/content/tests/BrowserTestUtils/content-utils.js", true);


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
   * @param {Object} options
   *        {
   *          private: A boolean indicating if the window should be
   *                   private
   *        }
   * @return {Promise}
   *         Resolves with the new window once it is loaded.
   */
  openNewBrowserWindow(options) {
    return new Promise(resolve => {
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
      TestUtils.topicObserved("browser-delayed-startup-finished", win).then(
        () => resolve(win));
    });
  },
};
