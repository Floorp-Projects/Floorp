/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TargetList"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { MessagePromise } = ChromeUtils.import(
  "chrome://remote/content/Sync.jsm"
);
const { TabTarget } = ChromeUtils.import(
  "chrome://remote/content/targets/TabTarget.jsm"
);
const { MainProcessTarget } = ChromeUtils.import(
  "chrome://remote/content/targets/MainProcessTarget.jsm"
);
const { TabObserver } = ChromeUtils.import(
  "chrome://remote/content/observers/TargetObserver.jsm"
);

class TargetList {
  constructor() {
    // Target ID -> Target
    this._targets = new Map();

    EventEmitter.decorate(this);
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
    this.tabObserver = new TabObserver({ registerExisting: true });
    this.tabObserver.on("open", async (eventName, tab) => {
      const browser = tab.linkedBrowser;
      // The tab may just have been created and not fully initialized yet.
      // Target class expects BrowserElement.browsingContext to be defined
      // whereas it is asynchronously set by the custom element class.
      // At least ensure that this property is set before instantiating the target.
      if (!browser.browsingContext) {
        await new MessagePromise(browser.messageManager, "Browser:Init");
      }
      const target = new TabTarget(this, browser);
      this.registerTarget(target);
    });
    this.tabObserver.on("close", (eventName, tab) => {
      const browser = tab.linkedBrowser;
      // Ignore the browsers that haven't had time to initialize.
      if (!browser.browsingContext) {
        return;
      }
      const target = this.getByBrowsingContext(browser.browsingContext.id);
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
   * Get Target instance by browsing context id
   *
   * @param {number} id
   *     browsing context id
   *
   * @return {Target}
   */
  getByBrowsingContext(id) {
    let rv;
    for (const target of this._targets.values()) {
      if (target.browsingContext && target.browsingContext.id === id) {
        rv = target;
        break;
      }
    }
    return rv;
  }

  /**
   * Get the Target instance for the main process.
   * This target is a singleton and only exposes a subset of domains.
   */
  getMainProcessTarget() {
    if (!this.mainProcessTarget) {
      this.mainProcessTarget = new MainProcessTarget(this);
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
