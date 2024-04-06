/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This class manages the communications to the translations engine via MessagePort.
 */
export class Translator {
  /**
   * The port through with to communicate with the Translations Engine.
   *
   * @type {MessagePort}
   */
  #port;

  /**
   * True if the current #port is closed, otherwise false.
   *
   * @type {boolean}
   */
  #portClosed = true;

  /**
   * A promise that resolves when the Translator has successfully established communication with
   * the translations engine, or rejects if communication was not successfully established.
   *
   * @type {Promise<void>}
   */
  #ready = Promise.reject;

  /**
   * The BCP-47 language tag for the from-language.
   *
   * @type {string}
   */
  #fromLanguage;

  /**
   * The BCP-47 language tag for the to-language.
   *
   * @type {string}
   */
  #toLanguage;

  /**
   * The callback function to request a new port, provided at construction time
   * by the caller.
   *
   * @type {Function}
   */
  #requestTranslationsPort;

  /**
   * An id for each message sent. This is used to match up the request and response.
   *
   * @type {number}
   */
  #nextMessageId = 0;

  /**
   * Tie together a message id to a resolved response.
   *
   * @type {Map<number, TranslationRequest>}
   */
  #requests = new Map();

  /**
   * Initializes a new Translator.
   *
   * Prefer using the Translator.create() function.
   *
   * @see Translator.create
   *
   * @param {string} fromLanguage - The BCP-47 from-language tag.
   * @param {string} toLanguage - The BCP-47 to-language tag.
   * @param {Function} requestTranslationsPort - A callback function to request a new MessagePort.
   */
  constructor(fromLanguage, toLanguage, requestTranslationsPort) {
    this.#fromLanguage = fromLanguage;
    this.#toLanguage = toLanguage;
    this.#requestTranslationsPort = requestTranslationsPort;
    this.#createNewPortIfClosed();
  }

  /**
   * @returns {Promise<void>} A promise that indicates if the Translator is ready to translate.
   */
  get ready() {
    return this.#ready;
  }

  /**
   * @returns {boolean} True if the translation port is closed, false otherwise.
   */
  get portClosed() {
    return this.#portClosed;
  }

  /**
   * @returns {string} The BCP-47 language tag of the from-language.
   */
  get fromLanguage() {
    return this.#fromLanguage;
  }

  /**
   * @returns {string} The BCP-47 language tag of the to-language.
   */
  get toLanguage() {
    return this.#toLanguage;
  }

  /**
   * Opens up a port and creates a new translator.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {Promise<Translator>}
   */
  static async create(fromLanguage, toLanguage, requestTranslationsPort) {
    if (!fromLanguage || !toLanguage || !requestTranslationsPort) {
      return undefined;
    }

    const translator = new Translator(
      fromLanguage,
      toLanguage,
      requestTranslationsPort
    );
    await translator.ready;

    return translator;
  }

  /**
   * Creates a new translation port if the current one is closed.
   *
   * @returns {Promise<void>} - Whether the Translator is ready to translate.
   */
  async #createNewPortIfClosed() {
    if (!this.#portClosed) {
      return this.#ready;
    }

    this.#port = await this.#requestTranslationsPort(
      this.#fromLanguage,
      this.#toLanguage
    );

    // Create a promise that will be resolved when the engine is ready.
    const { promise, resolve, reject } = Promise.withResolvers();

    // Match up a response on the port to message that was sent.
    this.#port.onmessage = ({ data }) => {
      switch (data.type) {
        case "TranslationsPort:TranslationResponse": {
          const { targetText, messageId } = data;
          // A request may not match match a messageId if there is a race during the pausing
          // and discarding of the queue.
          this.#requests.get(messageId)?.resolve(targetText);
          break;
        }
        case "TranslationsPort:GetEngineStatusResponse": {
          if (data.status === "ready") {
            this.#portClosed = false;
            resolve();
          } else {
            this.#portClosed = true;
            reject();
          }
          break;
        }
        case "TranslationsPort:EngineTerminated": {
          this.#portClosed = true;
          break;
        }
        default:
          break;
      }
    };

    this.#ready = promise;
    this.#port.postMessage({ type: "TranslationsPort:GetEngineStatusRequest" });

    return this.#ready;
  }

  /**
   * Send a request to translate text to the Translations Engine. If it returns `null`
   * then the request is stale. A rejection means there was an error in the translation.
   * This request may be queued.
   *
   * @param {string} sourceText
   * @returns {Promise<string>}
   */
  async translate(sourceText, isHTML = false) {
    await this.#createNewPortIfClosed();
    const { promise, resolve, reject } = Promise.withResolvers();
    const messageId = this.#nextMessageId++;

    // Store the "resolve" for the promise. It will be matched back up with the
    // `messageId` in #handlePortMessage.
    this.#requests.set(messageId, {
      sourceText,
      isHTML,
      resolve,
      reject,
    });
    this.#port.postMessage({
      type: "TranslationsPort:TranslationRequest",
      messageId,
      sourceText,
      isHTML,
    });

    return promise;
  }

  /**
   * Close the port and remove any pending or queued requests.
   */
  destroy() {
    this.#port.close();
    this.#portClosed = true;
    this.#ready = Promise.reject;
  }
}
