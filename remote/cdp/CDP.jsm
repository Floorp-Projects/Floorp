/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CDP"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  JSONHandler: "chrome://remote/content/cdp/JSONHandler.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  RecommendedPreferences:
    "chrome://remote/content/shared/RecommendedPreferences.jsm",
  TargetList: "chrome://remote/content/cdp/targets/TargetList.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.CDP)
);
XPCOMUtils.defineLazyGetter(lazy, "textEncoder", () => new TextEncoder());

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
    this._activePortPath;
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

    lazy.RecommendedPreferences.applyPreferences(RECOMMENDED_PREFS);

    // Starting CDP too early can cause issues with clients in not being able
    // to find any available target. Also when closing the application while
    // it's still starting up can cause shutdown hangs. As such CDP will be
    // started when the initial application window has finished initializing.
    lazy.logger.debug(`Waiting for initial application window`);
    await this.agent.browserStartupFinished;

    this.agent.server.registerPrefixHandler(
      "/json/",
      new lazy.JSONHandler(this)
    );

    this.targetList = new lazy.TargetList();
    this.targetList.on("target-created", (eventName, target) => {
      this.agent.server.registerPathHandler(target.path, target);
    });
    this.targetList.on("target-destroyed", (eventName, target) => {
      this.agent.server.registerPathHandler(target.path, null);
    });

    await this.targetList.watchForTargets();

    Cu.printStderr(`DevTools listening on ${this.address}\n`);

    // Write connection details to DevToolsActivePort file within the profile.
    this._activePortPath = PathUtils.join(
      PathUtils.profileDir,
      "DevToolsActivePort"
    );

    const data = `${this.agent.port}\n${this.mainTargetPath}`;
    try {
      await IOUtils.write(this._activePortPath, lazy.textEncoder.encode(data));
    } catch (e) {
      lazy.logger.warn(
        `Failed to create ${this._activePortPath} (${e.message})`
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
        await IOUtils.remove(this._activePortPath);
      } catch (e) {
        lazy.logger.warn(
          `Failed to remove ${this._activePortPath} (${e.message})`
        );
      }

      this.targetList?.destructor();
      this.targetList = null;

      lazy.RecommendedPreferences.restorePreferences(RECOMMENDED_PREFS);
    } finally {
      this._running = false;
    }
  }
}
