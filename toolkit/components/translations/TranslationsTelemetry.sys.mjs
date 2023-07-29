/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Telemetry functions for Translations actors
 */
export class TranslationsTelemetry {
  /**
   * A cached value to hold the current flowId.
   */
  static #flowId = null;

  /**
   * Telemetry functions for the Translations panel.
   * @returns {Panel}
   */
  static panel() {
    return Panel;
  }

  /**
   * Forces the creation of a new Translations telemetry flowId and returns it.
   * @returns {string}
   */
  static createFlowId() {
    const flowId = crypto.randomUUID();
    TranslationsTelemetry.#flowId = flowId;
    return flowId;
  }

  /**
   * Returns a Translations telemetry flowId by retrieving the cached value
   * if available, or creating a new one otherwise.
   * @returns {string}
   */
  static getOrCreateFlowId() {
    // If we have the flowId cached, return it.
    if (TranslationsTelemetry.#flowId) {
      return TranslationsTelemetry.#flowId;
    }

    // If no flowId exists, create one.
    return TranslationsTelemetry.createFlowId();
  }

  /**
   * Records a telemetry event when full page translation fails.
   *
   * @param {string} errorMessage
   */
  static onError(errorMessage) {
    Glean.translations.errorRate.addToNumerator(1);
    Glean.translations.error.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
      reason: errorMessage,
    });
  }

  /**
   * Records a telemetry event when a translation request is sent.
   *
   * @param {object} data
   * @param {string} data.fromLanguage
   * @param {string} data.toLanguage
   * @param {boolean} data.autoTranslate
   */
  static onTranslate(data) {
    Glean.translations.requestsCount.add(1);
    Glean.translations.translationRequest.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
      from_language: data.fromLanguage,
      to_language: data.toLanguage,
      auto_translate: data.autoTranslate,
    });
  }

  static onRestorePage() {
    Glean.translations.restorePage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
    });
  }
}

/**
 * Telemetry functions for the Translations panel
 */
class Panel {
  /**
   * A value to retain whether this is the user's first time
   * interacting with the translations panel. It is propagated
   * to all events.
   *
   * This value is set only through the onOpen() function.
   */
  static #isFirstUserInteraction = false;

  /**
   * True if this is the user's first time interacting with the
   * Translations panel, otherwise false.
   *
   * @returns {boolean}
   */
  static isFirstUserInteraction() {
    return Panel.#isFirstUserInteraction;
  }

  /**
   * Records a telemetry event when the translations panel is opened.
   *
   * @param {object} data
   * @param {boolean} data.maintainFlow
   * @param {boolean} data.openedFromAppMenu
   * @param {boolean} data.isFirstUserInteraction
   */
  static onOpen({
    maintainFlow = false,
    openedFromAppMenu = false,
    isFirstUserInteraction = null,
  }) {
    if (isFirstUserInteraction !== null || !maintainFlow) {
      Panel.#isFirstUserInteraction = isFirstUserInteraction ?? false;
    }
    Glean.translationsPanel.open.record({
      flow_id: maintainFlow
        ? TranslationsTelemetry.getOrCreateFlowId()
        : TranslationsTelemetry.createFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
      opened_from: openedFromAppMenu ? "appMenu" : "translationsButton",
    });
  }

  static onCancelButton() {
    Glean.translationsPanel.cancelButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
    });
  }

  static onChangeSourceLanguageButton() {
    Glean.translationsPanel.changeSourceLanguageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
    });
  }

  static onDismissErrorButton() {
    Glean.translationsPanel.dismissErrorButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
    });
  }

  static onRestorePageButton() {
    Glean.translationsPanel.restorePageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
    });
  }

  static onTranslateButton() {
    Glean.translationsPanel.translateButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      first_interaction: Panel.isFirstUserInteraction(),
    });
  }
}
