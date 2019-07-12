/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Input"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);

class Input extends ContentProcessDomain {
  constructor(session) {
    super(session);

    // Map of event name -> event handler promise.
    this._eventPromises = new Map();
  }

  /**
   * Not a CDP method.
   *
   * Add an event listener in the content page for the provided eventName.
   * To wait for the event, you should use `waitForContentEvent` with the same event name.
   *
   * Example usage from a parent process domain:
   *
   *   await this.executeInChild("addContentEventListener", "click");
   *   // do something that triggers a click in content
   *   await this.executeInChild("waitForContentEvent", "click");
   */
  addContentEventListener(eventName) {
    const eventPromise = new Promise(r => {
      this.chromeEventHandler.addEventListener(eventName, r, {
        mozSystemGroup: true,
        once: true,
      });
    });
    this._eventPromises.set(eventName, eventPromise);
  }

  /**
   * Not a CDP method.
   *
   * Wait for an event listener added via `addContentEventListener` to be fired.
   */
  async waitForContentEvent(eventName) {
    const eventPromise = this._eventPromises.get(eventName);
    if (!eventPromise) {
      throw new Error("No event promise available for " + eventName);
    }
    await eventPromise;
    this._eventPromises.delete(eventName);
  }
}
