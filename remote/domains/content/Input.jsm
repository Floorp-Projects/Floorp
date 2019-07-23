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

    // Internal id used to track existing event handlers.
    this._eventId = 0;

    // Map of event id -> event handler promise.
    this._eventPromises = new Map();
  }

  /**
   * Not a CDP method.
   *
   * Add an event listener in the content page for the provided eventName.
   * This method will return a unique handler id that can be used to wait
   * for the event.
   *
   * Example usage from a parent process domain:
   *
   *   const id = await this.executeInChild("addContentEventListener", "click");
   *   // do something that triggers a click in content
   *   await this.executeInChild("waitForContentEvent", id);
   */
  addContentEventListener(eventName) {
    const eventPromise = new Promise(r => {
      this.chromeEventHandler.addEventListener(eventName, r, {
        mozSystemGroup: true,
        once: true,
      });
    });
    this._eventId++;
    this._eventPromises.set(this._eventId, eventPromise);
    return this._eventId;
  }

  /**
   * Not a CDP method.
   *
   * Wait for an event listener added via `addContentEventListener` to be fired.
   */
  async waitForContentEvent(eventId) {
    const eventPromise = this._eventPromises.get(eventId);
    if (!eventPromise) {
      throw new Error("No event promise found for id " + eventId);
    }
    await eventPromise;
    this._eventPromises.delete(eventId);
  }
}
