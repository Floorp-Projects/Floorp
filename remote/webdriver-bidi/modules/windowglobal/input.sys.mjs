/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  action: "chrome://remote/content/shared/webdriver/Actions.sys.mjs",
});

class InputModule extends Module {
  #actionState;

  constructor(messageHandler) {
    super(messageHandler);

    this.#actionState = null;
  }

  async performActions(options) {
    const { actions } = options;
    if (this.#actionState === null) {
      this.#actionState = new lazy.action.State({
        specCompatPointerOrigin: true,
      });
    }
    const actionChain = lazy.action.Chain.fromJSON(this.#actionState, actions);

    await actionChain.dispatch(this.#actionState, this.messageHandler.window);
  }
}

export const input = InputModule;
