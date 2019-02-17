/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Targets"];

const {Target} = ChromeUtils.import("chrome://remote/content/Target.jsm");

class Targets {
  constructor() {
    // browser context ID -> Target<XULElement>
    this._targets = new Map();
  }

  /** @param BrowserElement browser */
  async connect(browser) {
    // The tab may just have been created and not fully initialized yet.
    // Target class expects BrowserElement.browsingContext to be defined
    // whereas it is asynchronously set by the custom element class.
    // At least ensure that this property is set before instantiating the target.
    if (!browser.browsingContext) {
      await new Promise(resolve => {
        const onInit = () => {
          browser.messageManager.removeMessageListener("Browser:Init", onInit);
          resolve();
        };
        browser.messageManager.addMessageListener("Browser:Init", onInit);
      });
    }
    const target = new Target(browser);

    target.connect();
    this._targets.set(target.id, target);
  }

  /** @param BrowserElement browser */
  disconnect(browser) {
    // Ignore the browsers that haven't had time to initialize.
    if (!browser.browsingContext) {
      return;
    }
    let target = this._targets.get(browser.browsingContext.id);

    if (target) {
      target.disconnect();
      this._targets.delete(target.id);
    }
  }

  clear() {
    for (const target of this) {
      this.disconnect(target.browser);
    }
  }

  get size() {
    return this._targets.size;
  }

  * [Symbol.iterator]() {
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
