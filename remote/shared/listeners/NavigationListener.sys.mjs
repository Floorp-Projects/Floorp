/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
});

/**
 * The NavigationListener simply wraps a NavigationManager instance and exposes
 * it with a convenient listener API, more consistent with the rest of the
 * remote codebase. NavigationManager is a singleton per session so it can't
 * be instanciated for each and every consumer.
 *
 * Example:
 * ```
 * const onNavigationStarted = (eventName, data = {}) => {
 *   const { level, message, stacktrace, timestamp } = data;
 *   ...
 * };
 *
 * const listener = new NavigationListener(this.messageHandler.navigationManager);
 * listener.on("navigation-started", onNavigationStarted);
 * listener.startListening();
 * ...
 * listener.stopListening();
 * ```
 *
 * @fires message
 *    The NavigationListener emits "navigation-started", "location-changed" and
 *    "navigation-stopped" events, with the following object as payload:
 *      - {string} navigationId - The UUID for the navigation.
 *      - {string} navigableId - The UUID for the navigable.
 *      - {string} url - The target url for the navigation.
 */
export class NavigationListener {
  #listening;
  #navigationManager;

  /**
   * Create a new NavigationListener instance.
   *
   * @param {NavigationManager} navigationManager
   *     The underlying NavigationManager for this listener.
   */
  constructor(navigationManager) {
    lazy.EventEmitter.decorate(this);

    this.#listening = false;
    this.#navigationManager = navigationManager;
  }

  get listening() {
    return this.#listening;
  }

  destroy() {
    this.stopListening();
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    this.#navigationManager.on("navigation-started", this.#forwardEvent);
    this.#navigationManager.on("navigation-stopped", this.#forwardEvent);
    this.#navigationManager.on("location-changed", this.#forwardEvent);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    this.#navigationManager.off("navigation-started", this.#forwardEvent);
    this.#navigationManager.off("navigation-stopped", this.#forwardEvent);
    this.#navigationManager.off("location-changed", this.#forwardEvent);

    this.#listening = false;
  }

  #forwardEvent = (name, data) => {
    this.emit(name, data);
  };
}
