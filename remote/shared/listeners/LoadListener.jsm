/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["LoadListener"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
});

/**
 * The LoadListener can be used to listen for load events.
 *
 * Example:
 * ```
 * const listener = new LoadListener();
 * listener.on("DOMContentLoaded", onDOMContentLoaded);
 * listener.startListening();
 *
 * const onDOMContentLoaded = (eventName, data = {}) => {
 *   const { target } = data;
 *   ...
 * };
 * ```
 *
 * @emits message
 *    The LoadListener emits "DOMContentLoaded" and "load" events,
 *    with the following object as payload:
 *      - {Document} target
 *            The target document.
 */
class LoadListener {
  #abortController;
  #window;

  /**
   * Create a new LoadListener instance.
   */
  constructor(win) {
    lazy.EventEmitter.decorate(this);

    // Use an abort controller instead of removeEventListener because destroy
    // might be called close to the window global destruction.
    this.#abortController = null;

    this.#window = win;
  }

  destroy() {
    this.stopListening();
  }

  startListening() {
    if (this.#abortController) {
      return;
    }

    this.#abortController = new AbortController();
    this.#window.addEventListener(
      "DOMContentLoaded",
      this.#onDOMContentLoaded,
      {
        mozSystemGroup: true,
        signal: this.#abortController.signal,
      }
    );

    this.#window.addEventListener("load", this.#onLoad, {
      mozSystemGroup: true,
      signal: this.#abortController.signal,
    });
  }

  stopListening() {
    if (!this.#abortController) {
      return;
    }

    this.#abortController.abort();
    this.#abortController = null;
  }

  #onDOMContentLoaded = event => {
    this.emit("DOMContentLoaded", { target: event.target });
  };

  #onLoad = event => {
    this.emit("load", { target: event.target });
  };
}
