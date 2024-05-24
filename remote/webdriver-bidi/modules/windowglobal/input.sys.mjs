/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  action: "chrome://remote/content/shared/webdriver/Actions.sys.mjs",
  dom: "chrome://remote/content/shared/DOM.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  event: "chrome://remote/content/shared/webdriver/Event.sys.mjs",
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
      this.#actionState = new lazy.action.State();
    }

    await this.#deserializeActionOrigins(actions);
    const actionChain = lazy.action.Chain.fromJSON(this.#actionState, actions);

    await actionChain.dispatch(this.#actionState, this.messageHandler.window);

    // Terminate the current wheel transaction if there is one. Wheel
    // transactions should not live longer than a single action chain.
    ChromeUtils.endWheelTransaction();
  }

  async releaseActions() {
    if (this.#actionState === null) {
      return;
    }
    await this.#actionState.release(this.messageHandler.window);
    this.#actionState = null;
  }

  async setFiles(options) {
    const { element: sharedReference, files } = options;

    const element = await this.#deserializeElementSharedReference(
      sharedReference
    );

    if (
      !HTMLInputElement.isInstance(element) ||
      element.type !== "file" ||
      element.disabled
    ) {
      throw new lazy.error.UnableToSetFileInputError(
        `Element needs to be an <input> element with type "file" and not disabled`
      );
    }

    if (files.length > 1 && !element.hasAttribute("multiple")) {
      throw new lazy.error.UnableToSetFileInputError(
        `Element should have an attribute "multiple" set when trying to set more than 1 file`
      );
    }

    const fileObjects = [];
    for (const file of files) {
      try {
        fileObjects.push(await File.createFromFileName(file));
      } catch (e) {
        throw new lazy.error.UnsupportedOperationError(
          `Failed to add file ${file} (${e})`
        );
      }
    }

    const selectedFiles = Array.from(element.files);

    const intersection = fileObjects.filter(fileObject =>
      selectedFiles.some(
        selectedFile =>
          // Compare file fields to identify if the files are equal.
          // TODO: Bug 1883856. Add check for full path or use a different way
          // to compare files when it's available.
          selectedFile.name === fileObject.name &&
          selectedFile.size === fileObject.size &&
          selectedFile.type === fileObject.type
      )
    );

    if (
      intersection.length === selectedFiles.length &&
      selectedFiles.length === fileObjects.length
    ) {
      lazy.event.cancel(element);
    } else {
      element.mozSetFileArray(fileObjects);

      lazy.event.input(element);
      lazy.event.change(element);
    }
  }

  /**
   * In the provided array of input.SourceActions, replace all origins matching
   * the input.ElementOrigin production with the Element corresponding to this
   * origin.
   *
   * Note that this method replaces the content of the `actions` in place, and
   * does not return a new array.
   *
   * @param {Array<input.SourceActions>} actions
   *     The array of SourceActions to deserialize.
   * @returns {Promise}
   *     A promise which resolves when all ElementOrigin origins have been
   *     deserialized.
   */
  async #deserializeActionOrigins(actions) {
    const promises = [];

    if (!Array.isArray(actions)) {
      // Silently ignore invalid action chains because they are fully parsed later.
      return Promise.resolve();
    }

    for (const actionsByTick of actions) {
      if (!Array.isArray(actionsByTick?.actions)) {
        // Silently ignore invalid actions because they are fully parsed later.
        return Promise.resolve();
      }

      for (const action of actionsByTick.actions) {
        if (action?.origin?.type === "element") {
          promises.push(
            (async () => {
              action.origin = await this.#deserializeElementSharedReference(
                action.origin.element
              );
            })()
          );
        }
      }
    }

    return Promise.all(promises);
  }

  async #deserializeElementSharedReference(sharedReference) {
    if (typeof sharedReference?.sharedId !== "string") {
      throw new lazy.error.InvalidArgumentError(
        `Expected "element" to be a SharedReference, got: ${sharedReference}`
      );
    }

    const realm = this.messageHandler.getRealm();

    const element = this.deserialize(sharedReference, realm);
    if (!lazy.dom.isElement(element)) {
      throw new lazy.error.NoSuchElementError(
        `No element found for shared id: ${sharedReference.sharedId}`
      );
    }

    return element;
  }
}

export const input = InputModule;
