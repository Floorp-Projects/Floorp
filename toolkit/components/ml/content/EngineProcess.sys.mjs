/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  HiddenFrame: "resource://gre/modules/HiddenFrame.sys.mjs",
});

/**
 * @typedef {import("../actors/MLEngineParent.sys.mjs").MLEngineParent} MLEngineParent
 */

/**
 * @typedef {import("../../translations/actors/TranslationsEngineParent.sys.mjs").TranslationsEngineParent} TranslationsEngineParent
 */

/**
 * This class controls the life cycle of the engine process used both in the
 * Translations engine and the MLEngine component.
 */
export class EngineProcess {
  /**
   * @type {Promise<{ hiddenFrame: HiddenFrame, actor: TranslationsEngineParent }> | null}
   */

  /** @type {Promise<HiddenFrame> | null} */
  static #hiddenFrame = null;
  /** @type {Promise<TranslationsEngineParent> | null} */
  static translationsEngineParent = null;
  /** @type {Promise<MLEngineParent> | null} */
  static mlEngineParent = null;

  /** @type {((actor: TranslationsEngineParent) => void) | null} */
  resolveTranslationsEngineParent = null;

  /** @type {((actor: MLEngineParent) => void) | null} */
  resolveMLEngineParent = null;

  /**
   * See if all engines are terminated. This is useful for testing.
   *
   * @returns {boolean}
   */
  static areAllEnginesTerminated() {
    return (
      !EngineProcess.#hiddenFrame &&
      !EngineProcess.translationsEngineParent &&
      !EngineProcess.mlEngineParent
    );
  }

  /**
   * @returns {Promise<TranslationsEngineParent>}
   */
  static async getTranslationsEngineParent() {
    if (!this.translationsEngineParent) {
      this.translationsEngineParent = this.#attachBrowser({
        id: "translations-engine-browser",
        url: "chrome://global/content/translations/translations-engine.html",
        resolverName: "resolveTranslationsEngineParent",
      });
    }
    return this.translationsEngineParent;
  }

  /**
   * @returns {Promise<MLEngineParent>}
   */
  static async getMLEngineParent() {
    if (!this.mlEngineParent) {
      this.mlEngineParent = this.#attachBrowser({
        id: "ml-engine-browser",
        url: "chrome://global/content/ml/MLEngine.html",
        resolverName: "resolveMLEngineParent",
      });
    }
    return this.mlEngineParent;
  }

  /**
   * @param {object} config
   * @param {string} config.url
   * @param {string} config.id
   * @param {string} config.resolverName
   * @returns {Promise<TranslationsEngineParent>}
   */
  static async #attachBrowser({ url, id, resolverName }) {
    const hiddenFrame = await this.#getHiddenFrame();
    const chromeWindow = await hiddenFrame.get();
    const doc = chromeWindow.document;

    if (doc.getElementById(id)) {
      throw new Error(
        "Attempting to append the translations-engine.html <browser> when one " +
          "already exists."
      );
    }

    const browser = doc.createXULElement("browser");
    browser.setAttribute("id", id);
    browser.setAttribute("remote", "true");
    browser.setAttribute("remoteType", "web");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute("src", url);

    ChromeUtils.addProfilerMarker(
      "EngineProcess",
      {},
      `Creating the "${id}" process`
    );
    doc.documentElement.appendChild(browser);

    const { promise, resolve } = Promise.withResolvers();

    // The engine parents must resolve themselves when they are ready.
    this[resolverName] = resolve;

    return promise;
  }

  /**
   * @returns {HiddenFrame}
   */
  static async #getHiddenFrame() {
    if (!EngineProcess.#hiddenFrame) {
      EngineProcess.#hiddenFrame = new lazy.HiddenFrame();
    }
    return EngineProcess.#hiddenFrame;
  }

  /**
   * Destroy the translations engine, and remove the hidden frame if no other
   * engines exist.
   */
  static destroyTranslationsEngine() {
    return this.#destroyEngine({
      id: "translations-engine-browser",
      keyName: "translationsEngineParent",
    });
  }

  /**
   * Destroy the ML engine, and remove the hidden frame if no other engines exist.
   */
  static destroyMLEngine() {
    return this.#destroyEngine({
      id: "ml-engine-browser",
      keyName: "mlEngineParent",
    });
  }

  /**
   * Destroy the specified engine and maybe the entire hidden frame as well if no engines
   * are remaining.
   */
  static #destroyEngine({ id, keyName }) {
    ChromeUtils.addProfilerMarker(
      "EngineProcess",
      {},
      `Destroying the "${id}" engine`
    );

    const actorShutdown = this.forceActorShutdown(id, keyName).catch(
      error => void console.error(error)
    );

    this[keyName] = null;

    const hiddenFrame = EngineProcess.#hiddenFrame;
    if (hiddenFrame && !this.translationsEngineParent && !this.mlEngineParent) {
      EngineProcess.#hiddenFrame = null;

      // Both actors are destroyed, also destroy the hidden frame.
      actorShutdown.then(() => {
        // Double check a race condition that no new actors have been created during
        // shutdown.
        if (this.translationsEngineParent && this.mlEngineParent) {
          return;
        }
        if (!hiddenFrame) {
          return;
        }
        hiddenFrame.destroy();
        ChromeUtils.addProfilerMarker(
          "EngineProcess",
          {},
          `Removing the hidden frame`
        );
      });
    }

    // Infallibly resolve the promise even if there are errors.
    return Promise.resolve();
  }

  /**
   * Shut down an actor and remove its <browser> element.
   *
   * @param {string} id
   * @param {string} keyName
   */
  static async forceActorShutdown(id, keyName) {
    const actorPromise = this[keyName];
    if (!actorPromise) {
      return;
    }

    let actor;
    try {
      actor = await actorPromise;
    } catch {
      // The actor failed to initialize, so it doesn't need to be shut down.
      return;
    }

    // Shut down the actor.
    try {
      await actor.forceShutdown();
    } catch (error) {
      console.error("Failed to shut down the actor " + id, error);
      return;
    }

    if (!EngineProcess.#hiddenFrame) {
      // The hidden frame was already removed.
      return;
    }

    // Remove the <brower> element.
    const chromeWindow = EngineProcess.#hiddenFrame.getWindow();
    const doc = chromeWindow.document;
    const element = doc.getElementById(id);
    if (!element) {
      console.error("Could not find the <browser> element for " + id);
      return;
    }
    element.remove();
  }
}
