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
  RecommendedPreferences:
    "chrome://remote/content/shared/RecommendedPreferences.jsm",
  TargetList: "chrome://remote/content/cdp/targets/TargetList.jsm",
});

// Map of CDP-specific preferences that should be set via
// RecommendedPreferences.
const RECOMMENDED_PREFS = new Map([
  // Prevent various error message on the console
  // jest-puppeteer asserts that no error message is emitted by the console
  [
    "browser.contentblocking.features.standard",
    "-tp,tpPrivate,cookieBehavior0,-cm,-fp",
  ],
  // Accept all cookies (see behavior definitions in nsICookieService.idl)
  ["network.cookie.cookieBehavior", 0],
]);

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
   * @param {RemoteAgent} agent
   *     Reference to the Remote Agent instance.
   */
  constructor(agent) {
    this.agent = agent;
    this.targetList = null;
  }

  get address() {
    const mainTarget = this.targetList.getMainProcessTarget();
    return mainTarget.wsDebuggerURL;
  }

  /**
   * Starts the CDP support.
   */
  async start() {
    RecommendedPreferences.applyPreferences(RECOMMENDED_PREFS);

    this.agent.server.registerPrefixHandler("/json/", new JSONHandler(this));

    this.targetList = new TargetList();
    this.targetList.on("target-created", (eventName, target) => {
      this.agent.server.registerPathHandler(target.path, target);
    });
    this.targetList.on("target-destroyed", (eventName, target) => {
      this.agent.server.registerPathHandler(target.path, null);
    });

    await this.targetList.watchForTargets();

    // Immediatly instantiate the main process target in order
    // to be accessible via HTTP endpoint on startup

    Services.obs.notifyObservers(
      null,
      "remote-listening",
      `DevTools listening on ${this.address}`
    );
  }

  /**
   * Stops the CDP support.
   */
  stop() {
    this.targetList.destructor();
    this.targetList = null;

    RecommendedPreferences.restorePreferences(RECOMMENDED_PREFS);
  }
}
