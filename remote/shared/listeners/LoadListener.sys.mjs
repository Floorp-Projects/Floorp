/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
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
 * @fires message
 *    The LoadListener emits "DOMContentLoaded" and "load" events,
 *    with the following object as payload:
 *      - {Document} target
 *            The target document.
 */
export class LoadListener {
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

    // Events are attached to the windowRoot instead of the regular window to
    // avoid issues with document.open (Bug 1822772).
    this.#window.windowRoot.addEventListener(
      "DOMContentLoaded",
      this.#onDOMContentLoaded,
      {
        capture: true,
        mozSystemGroup: true,
        signal: this.#abortController.signal,
      }
    );

    this.#window.windowRoot.addEventListener("load", this.#onLoad, {
      capture: true,
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
    // Check that this event was emitted for the relevant window, because events
    // from inner frames can bubble to the windowRoot.
    if (event.target.defaultView === this.#window) {
      this.emit("DOMContentLoaded", { target: event.target });
    }
  };

  #onLoad = event => {
    // Check that this event was emitted for the relevant window, because events
    // from inner frames can bubble to the windowRoot.
    if (event.target.defaultView === this.#window) {
      this.emit("load", { target: event.target });
    }
  };
}
