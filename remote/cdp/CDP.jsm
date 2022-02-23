/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CDP"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  JSONHandler: "chrome://remote/content/cdp/JSONHandler.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  RecommendedPreferences:
    "chrome://remote/content/shared/RecommendedPreferences.jsm",
  TargetList: "chrome://remote/content/cdp/targets/TargetList.jsm",
});

XPCOMUtils.defineLazyGetter(this, "textEncoder", () => new TextEncoder());

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get(Log.TYPES.CDP));

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

    this._running = false;
    this._devToolsActivePortPath;
  }

  get address() {
    const mainTarget = this.targetList.getMainProcessTarget();
    return mainTarget.wsDebuggerURL;
  }

  get mainTargetPath() {
    const mainTarget = this.targetList.getMainProcessTarget();
    return mainTarget.path;
  }

  /**
   * Starts the CDP support.
   */
  async start() {
    if (this._running) {
      return;
    }

    // Note: Ideally this would only be set at the end of the method. However
    // since start() is async, we prefer to set the flag early in order to
    // avoid potential race conditions.
    this._running = true;

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

    Cu.printStderr(`DevTools listening on ${this.address}\n`);

    // Write connection details to DevToolsActivePort file within the profile
    const profileDir = await PathUtils.getProfileDir();
    this._devToolsActivePortPath = PathUtils.join(
      profileDir,
      "DevToolsActivePort"
    );

    const data = `${this.agent.port}\n${this.mainTargetPath}`;
    try {
      await IOUtils.write(
        this._devToolsActivePortPath,
        textEncoder.encode(data)
      );
    } catch (e) {
      logger.warn(
        `Failed to create ${this._devToolsActivePortPath} (${e.message})`
      );
    }
  }

  /**
   * Stops the CDP support.
   */
  async stop() {
    if (!this._running) {
      return;
    }

    try {
      try {
        await IOUtils.remove(this._devToolsActivePortPath);
      } catch (e) {
        logger.warn(
          `Failed to remove ${this._devToolsActivePortPath} (${e.message})`
        );
      }

      this.targetList?.destructor();
      this.targetList = null;

      RecommendedPreferences.restorePreferences(RECOMMENDED_PREFS);
    } finally {
      this._running = false;
    }
  }
}
