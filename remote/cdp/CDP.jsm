/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CDP"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  JSONHandler: "chrome://remote/content/cdp/JSONHandler.jsm",
  TargetList: "chrome://remote/content/cdp/targets/TargetList.jsm",
});

/**
 * Entry class for the Chrome DevTools Protocol support.
 *
 * It holds the list of available targets (tabs, main browser), and also
 * sets up the necessary handlers for the HTTP server.
 *
 * @see https://chromedevtools.github.io/devtools-protocol
 */
class CDP {
  /**
   * Creates a new instance of the CDP class.
   *
   * @param {HttpServer} server
   *     The HTTP server that handles new connection requests.
   */
  constructor(server) {
    this.server = server;

    this.server.registerPrefixHandler("/json/", new JSONHandler(this));

    this.targetList = new TargetList();
    this.targetList.on("target-created", (eventName, target) => {
      this.server.registerPathHandler(target.path, target);
    });
    this.targetList.on("target-destroyed", (eventName, target) => {
      this.server.registerPathHandler(target.path, null);
    });
  }

  /**
   * Starts the CDP support.
   */
  async start() {
    await this.targetList.watchForTargets();

    // Immediatly instantiate the main process target in order
    // to be accessible via HTTP endpoint on startup
    const mainTarget = this.targetList.getMainProcessTarget();
    Services.obs.notifyObservers(
      null,
      "remote-listening",
      `DevTools listening on ${mainTarget.wsDebuggerURL}`
    );
  }

  /**
   * Stops the CDP support.
   */
  stop() {
    this.targetList.destructor();
  }
}
