/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTabUtil"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
});

const GeckoViewTabUtil = {
  /**
   * Creates a new tab through service worker delegate.
   * Needs to be ran in a parent process.
   *
   * @param {string} url
   * @returns {Tab}
   * @throws {Error} Throws an error if the tab cannot be created.
   */
  async createNewTab(url = "about:blank") {
    let sessionId = "";
    const windowPromise = new Promise(resolve => {
      const openingObserver = (subject, topic, data) => {
        if (subject.name === sessionId) {
          Services.obs.removeObserver(
            openingObserver,
            "geckoview-window-created"
          );
          resolve(subject);
        }
      };
      Services.obs.addObserver(openingObserver, "geckoview-window-created");
    });

    try {
      sessionId = await lazy.EventDispatcher.instance.sendRequestForResult({
        type: "GeckoView:Test:NewTab",
        url,
      });
    } catch (errorMessage) {
      throw new Error(
        errorMessage + " GeckoView:Test:NewTab is not supported."
      );
    }

    if (!sessionId) {
      throw new Error("Could not open a session for the new tab.");
    }

    const window = await windowPromise;
    return window.tab;
  },
};
