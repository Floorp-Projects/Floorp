/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

/**
 * The translations engine is in its own content process. This actor handles the
 * marshalling of the data such as the engine payload and port passing.
 */
export class TranslationsEngineParent extends JSWindowActorParent {
  /**
   * Keep track of the live actors by InnerWindowID.
   *
   * @type {Map<InnerWindowID, TranslationsParent | AboutTranslationsParent>}
   */
  #translationsParents = new Map();

  async receiveMessage({ name, data }) {
    switch (name) {
      case "TranslationsEngine:Ready":
        if (!lazy.TranslationsParent.resolveEngine) {
          throw new Error(
            "Unable to find the resolve function for when the translations engine is ready."
          );
        }
        lazy.TranslationsParent.resolveEngine(this);
        return undefined;
      case "TranslationsEngine:RequestEnginePayload": {
        const { fromLanguage, toLanguage } = data;
        const payloadPromise =
          lazy.TranslationsParent.getTranslationsEnginePayload(
            fromLanguage,
            toLanguage
          );
        payloadPromise.catch(error => {
          lazy.TranslationsParent.telemetry().onError(String(error));
        });
        return payloadPromise;
      }
      case "TranslationsEngine:ReportEngineStatus": {
        const { innerWindowId, status } = data;
        const translationsParent = this.#translationsParents.get(innerWindowId);

        // about:translations will not have a TranslationsParent associated with
        // this call.
        if (translationsParent) {
          switch (status) {
            case "ready":
              translationsParent.languageState.isEngineReady = true;
              break;
            case "error":
              translationsParent.languageState.error = "engine-load-failure";
              break;
            default:
              throw new Error("Unknown engine status: " + status);
          }
        }
        return undefined;
      }
      case "TranslationsEngine:DestroyEngineProcess":
        ChromeUtils.addProfilerMarker(
          "TranslationsEngine",
          {},
          "Loading bergamot wasm array buffer"
        );
        lazy.TranslationsParent.destroyEngineProcess().catch(error =>
          console.error(error)
        );
        return undefined;
      default:
        return undefined;
    }
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {number} innerWindowId
   * @param {MessagePort} port
   * @param {number} innerWindowId
   * @param {TranslationsParent} [translationsParent]
   */
  startTranslation(
    fromLanguage,
    toLanguage,
    port,
    innerWindowId,
    translationsParent
  ) {
    if (translationsParent) {
      this.#translationsParents.set(
        translationsParent.innerWindowId,
        translationsParent
      );
    }
    if (this.#isDestroyed) {
      throw new Error("The translation engine process was already destroyed.");
    }
    const transferables = [port];
    this.sendAsyncMessage(
      "TranslationsEngine:StartTranslation",
      {
        fromLanguage,
        toLanguage,
        innerWindowId,
        port,
      },
      transferables
    );
  }

  /**
   * Remove all the translations that are currently queued, and remove
   * the communication port.
   *
   * @param {number} innerWindowId
   */
  discardTranslations(innerWindowId) {
    this.#translationsParents.delete(innerWindowId);
    this.sendAsyncMessage("TranslationsEngine:DiscardTranslations", {
      innerWindowId,
    });
  }

  /**
   * Manually shut down the engines, typically for testing purposes.
   */
  forceShutdown() {
    return this.sendQuery("TranslationsEngine:ForceShutdown");
  }

  #isDestroyed = false;

  didDestroy() {
    this.#isDestroyed = true;
  }
}
