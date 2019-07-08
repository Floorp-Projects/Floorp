/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Targets"];

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

class Targets {
  constructor() {
    // browser context ID -> Target<XULElement>
    this._targets = new Map();

    EventEmitter.decorate(this);
  }

  /** @param {BrowserElement} browser */
  async connect(browser) {
    // The tab may just have been created and not fully initialized yet.
    // Target class expects BrowserElement.browsingContext to be defined
    // whereas it is asynchronously set by the custom element class.
    // At least ensure that this property is set before instantiating the target.
    if (!browser.browsingContext) {
      await new MessagePromise(browser.messageManager, "Browser:Init");
    }

    const target = new TabTarget(this, browser);
    target.connect();
    this._targets.set(target.id, target);
    this.emit("connect", target);
  }

  /** @param {BrowserElement} browser */
  disconnect(browser) {
    // Ignore the browsers that haven't had time to initialize.
    if (!browser.browsingContext) {
      return;
    }

    const target = this._targets.get(browser.browsingContext.id);
    if (target) {
      this.emit("disconnect", target);
      target.disconnect();
      this._targets.delete(target.id);
    }
  }

  clear() {
    for (const target of this) {
      // The main process target doesn't have a `browser` and so would fail here.
      // Ignore it here, and instead destroy it individually right after this.
      if (target != this.mainProcessTarget) {
        this.disconnect(target.browser);
      }
    }
    this._targets.clear();
    if (this.mainProcessTarget) {
      this.mainProcessTarget.disconnect();
      this.mainProcessTarget = null;
    }
  }

  get size() {
    return this._targets.size;
  }

  /**
   * Get Target instance by target id
   *
   * @param int id Target id
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
      this.mainProcessTarget = new MainProcessTarget(this);
      this._targets.set(this.mainProcessTarget.id, this.mainProcessTarget);
      this.emit("connect", this.mainProcessTarget);
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
    return `[object Targets ${this.size}]`;
  }
}
