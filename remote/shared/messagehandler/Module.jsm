/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Module"];

class Module {
  /**
   * Create a new module instance.
   *
   * @param {MessageHandler} messageHandler
   *     The MessageHandler instance which owns this Module instance.
   */
  constructor(messageHandler) {
    this._messageHandler = messageHandler;
  }

  /**
   * Clean-up the module instance.
   *
   * It's required to be implemented in the sub class.
   */
  destroy() {
    throw new Error("Not implemented");
  }

  get messageHandler() {
    return this._messageHandler;
  }
}
