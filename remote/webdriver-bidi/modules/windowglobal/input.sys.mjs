/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  action: "chrome://remote/content/shared/webdriver/Actions.sys.mjs",
  deserialize: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
});

class InputModule extends WindowGlobalBiDiModule {
  #actionState;

  constructor(messageHandler) {
    super(messageHandler);

    this.#actionState = null;
  }

  destroy() {}

  async performActions(options) {
    const { actions } = options;
    if (this.#actionState === null) {
      this.#actionState = new lazy.action.State({
        specCompatPointerOrigin: true,
      });
    }

    const actionChain = lazy.action.Chain.fromJSON(this.#actionState, actions, {
      getElementFromElementOrigin: this.#getElementFromElementOrigin,
    });

    await actionChain.dispatch(this.#actionState, this.messageHandler.window);
  }

  async releaseActions() {
    if (this.#actionState === null) {
      return;
    }
    await this.#actionState.release(this.messageHandler.window);
    this.#actionState = null;
  }

  #getElementFromElementOrigin = origin => {
    const sharedReference = origin.element;
    if (typeof sharedReference?.sharedId !== "string") {
      throw new lazy.error.InvalidArgumentError(
        `Expected "origin.element" to be a SharedReference, got: ${sharedReference}`
      );
    }

    const realm = this.messageHandler.getRealm();
    return lazy.deserialize(realm, sharedReference, {
      nodeCache: this.nodeCache,
    });
  };
}

export const input = InputModule;
