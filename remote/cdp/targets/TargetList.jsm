/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TargetList"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  MainProcessTarget:
    "chrome://remote/content/cdp/targets/MainProcessTarget.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
  TabObserver: "chrome://remote/content/cdp/observers/TargetObserver.jsm",
  TabTarget: "chrome://remote/content/cdp/targets/TabTarget.jsm",
});

class TargetList {
  constructor() {
    // Target ID -> Target
    this._targets = new Map();

    lazy.EventEmitter.decorate(this);
  }

  /**
   * Start listing and listening for all the debuggable targets
   */
  async watchForTargets() {
    await this.watchForTabs();
  }

  unwatchForTargets() {
    this.unwatchForTabs();
  }

  /**
   * Watch for all existing and new tabs being opened.
   * So that we can create the related TabTarget instance for
   * each of them.
   */
  async watchForTabs() {
    if (this.tabObserver) {
      throw new Error("Targets is already watching for new tabs");
    }

    this.tabObserver = new lazy.TabObserver({ registerExisting: true });

    // Handle creation of tab targets for opened tabs.
    this.tabObserver.on("open", async (eventName, tab) => {
      const target = new lazy.TabTarget(this, tab.linkedBrowser);
      this.registerTarget(target);
    });

    // Handle removal of tab targets when tabs are closed.
    this.tabObserver.on("close", (eventName, tab) => {
      const browser = tab.linkedBrowser;

      // Ignore unloaded tabs.
      if (browser.browserId === 0) {
        return;
      }

      const id = lazy.TabManager.getIdForBrowser(browser);
      const target = Array.from(this._targets.values()).find(
        target => target.id == id
      );
      if (target) {
        this.destroyTarget(target);
      }
    });
    await this.tabObserver.start();
  }

  unwatchForTabs() {
    if (this.tabObserver) {
      this.tabObserver.stop();
      this.tabObserver = null;
    }
  }

  /**
   * To be called right after instantiating a new Target instance.
   * This will hold the new instance in the list and notify about
   * its creation.
   */
  registerTarget(target) {
    this._targets.set(target.id, target);
    this.emit("target-created", target);
  }

  /**
   * To be called when the debuggable target has been destroy.
   * So that we can notify it no longer exists and disconnect
   * all connecting made to debug it.
   */
  destroyTarget(target) {
    target.destructor();
    this._targets.delete(target.id);
    this.emit("target-destroyed", target);
  }

  /**
   * Destroy all the registered target of all kinds.
   * This will end up dropping all connections made to debug any of them.
   */
  destructor() {
    for (const target of this) {
      this.destroyTarget(target);
    }
    this._targets.clear();
    if (this.mainProcessTarget) {
      this.mainProcessTarget = null;
    }

    this.unwatchForTargets();
  }

  get size() {
    return this._targets.size;
  }

  /**
   * Get Target instance by target id
   *
   * @param {string} id
   *     Target id
   *
   * @return {Target}
   */
  getById(id) {
    return this._targets.get(id);
  }

  /**
   * Get the Target instance for the main process.
   * This target is a singleton and only exposes a subset of domains.
   */
  getMainProcessTarget() {
    if (!this.mainProcessTarget) {
      this.mainProcessTarget = new lazy.MainProcessTarget(this);
      this.registerTarget(this.mainProcessTarget);
    }
    return this.mainProcessTarget;
  }

  *[Symbol.iterator]() {
    for (const target of this._targets.values()) {
      yield target;
    }
  }

  toJSON() {
    return [...this];
  }

  toString() {
    return `[object TargetList ${this.size}]`;
  }
}
