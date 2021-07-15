/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebDriverBiDi"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * Entry class for the WebDriver BiDi support.
 *
 * @see https://w3c.github.io/webdriver-bidi
 */
class WebDriverBiDi {
  /**
   * Creates a new instance of the WebDriverBiDi class.
   *
   * @param {RemoteAgent} agent
   *     Reference to the Remote Agent instance.
   */
  constructor(agent) {
    this.agent = agent;
    this._running = false;
  }

  get address() {
    return `ws://${this.agent.host}:${this.agent.port}`;
  }

  /**
   * Starts the WebDriver BiDi support.
   */
  start() {
    if (this._running) {
      return;
    }

    this._running = true;

    Services.obs.notifyObservers(
      null,
      "remote-listening",
      `WebDriver BiDi listening on ${this.address}`
    );
  }

  /**
   * Stops the WebDriver BiDi support.
   */
  stop() {
    if (!this._running) {
      return;
    }

    this._running = false;
  }
}
