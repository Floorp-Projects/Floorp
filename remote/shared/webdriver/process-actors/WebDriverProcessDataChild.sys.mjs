/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  NodeCache: "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

// Observer to clean-up element references for closed browsing contexts.
class BrowsingContextObserver {
  constructor(actor) {
    this.actor = actor;
  }

  async observe(subject, topic, data) {
    if (topic === "browsing-context-discarded") {
      this.actor.cleanUp({ browsingContext: subject });
    }
  }
}

export class WebDriverProcessDataChild extends JSProcessActorChild {
  #browsingContextObserver;
  #nodeCache;

  constructor() {
    super();

    // For now have a single reference store only. Once multiple WebDriver
    // sessions are supported, it needs to be hashed by the session id.
    this.#nodeCache = new lazy.NodeCache();

    // Register observer to cleanup element references when a browsing context
    // gets destroyed.
    this.#browsingContextObserver = new BrowsingContextObserver(this);
    Services.obs.addObserver(
      this.#browsingContextObserver,
      "browsing-context-discarded"
    );
  }

  actorCreated() {
    lazy.logger.trace(
      `WebDriverProcessData actor created for PID ${Services.appinfo.processID}`
    );
  }

  didDestroy() {
    Services.obs.removeObserver(
      this.#browsingContextObserver,
      "browsing-context-discarded"
    );
  }

  /**
   * Clean up all the process specific data.
   *
   * @param {object=} options
   * @param {BrowsingContext=} options.browsingContext
   *     If specified only clear data living in that browsing context.
   */
  cleanUp(options = {}) {
    const { browsingContext = null } = options;

    this.#nodeCache.clear({ browsingContext });
  }

  /**
   * Get the node cache.
   *
   * @returns {NodeCache}
   *     The cache containing DOM node references.
   */
  getNodeCache() {
    return this.#nodeCache;
  }

  async receiveMessage(msg) {
    switch (msg.name) {
      case "WebDriverProcessDataParent:CleanUp":
        return this.cleanUp(msg.data);
      default:
        return Promise.reject(
          new Error(`Unexpected message received: ${msg.name}`)
        );
    }
  }
}
