/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "toolkit.telemetry.translations.logLevel",
    prefix: "TranslationsTelemetry",
  });
});

/**
 * Telemetry functions for Translations actors
 */
export class TranslationsTelemetry {
  /**
   * A cached value to hold the current flowId.
   */
  static #flowId = null;

  /**
   * Logs the telemetry event to the console if enabled by toolkit.telemetry.translations.logLevel
   *
   * @param {Function} caller - The calling function.
   * @param {object} [data] - Optional data passed to telemetry.
   */
  static logEventToConsole(caller, data) {
    const id = TranslationsTelemetry.getOrCreateFlowId().substring(0, 5);
    lazy.console?.debug(
      `flowId[${id}]: ${caller.name}`,
      ...(data ? [data] : [])
    );
  }

  /**
   * Telemetry functions for the Full Page Translations panel.
   *
   * @returns {FullPageTranslationsPanelTelemetry}
   */
  static fullPagePanel() {
    return FullPageTranslationsPanelTelemetry;
  }

  /**
   * Telemetry functions for the SelectTranslationsPanel.
   *
   * @returns {SelectTranslationsPanelTelemetry}
   */
  static selectTranslationsPanel() {
    return SelectTranslationsPanelTelemetry;
  }

  /**
   * Forces the creation of a new Translations telemetry flowId and returns it.
   *
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
   *
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
      reason: errorMessage,
    });
    TranslationsTelemetry.logEventToConsole(TranslationsTelemetry.onError, {
      errorMessage,
    });
  }

  /**
   * Records a telemetry event when a full-page translation request is sent.
   *
   * @param {object} data
   * @param {boolean} data.autoTranslate
   * @param {string} data.docLangTag
   * @param {string} data.fromLanguage
   * @param {string} data.toLanguage
   * @param {string} data.topPreferredLanguage
   * @param {string} data.requestTarget
   * @param {number} data.sourceTextCodeUnits
   * @param {number} data.sourceTextWordCount
   */
  static onTranslate(data) {
    const {
      autoTranslate,
      docLangTag,
      fromLanguage,
      requestTarget,
      toLanguage,
      topPreferredLanguage,
      sourceTextCodeUnits,
      sourceTextWordCount,
    } = data;
    Glean.translations.requestsCount.add(1);
    Glean.translations.requestCount[requestTarget ?? "full_page"].add(1);
    Glean.translations.translationRequest.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      from_language: fromLanguage,
      to_language: toLanguage,
      auto_translate: autoTranslate,
      document_language: docLangTag,
      top_preferred_language: topPreferredLanguage,
      request_target: requestTarget ?? "full_page",
      source_text_code_units: sourceTextCodeUnits,
      source_text_word_count: sourceTextWordCount,
    });
    TranslationsTelemetry.logEventToConsole(
      TranslationsTelemetry.onTranslate,
      data
    );
  }

  static onRestorePage() {
    Glean.translations.restorePage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      TranslationsTelemetry.onRestorePage
    );
  }
}

/**
 * Telemetry functions for the FullPageTranslationsPanel UI
 */
class FullPageTranslationsPanelTelemetry {
  /**
   * Records a telemetry event when the FullPageTranslationsPanel is opened.
   *
   * @param {object} data
   * @param {string} data.viewName
   * @param {string} data.docLangTag
   * @param {boolean} data.autoShow
   * @param {boolean} data.maintainFlow
   * @param {boolean} data.openedFromAppMenu
   */
  static onOpen(data) {
    Glean.translationsPanel.open.record({
      flow_id: data.maintainFlow
        ? TranslationsTelemetry.getOrCreateFlowId()
        : TranslationsTelemetry.createFlowId(),
      auto_show: data.autoShow,
      view_name: data.viewName,
      document_language: data.docLangTag,
      opened_from: data.openedFromAppMenu ? "appMenu" : "translationsButton",
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onOpen,
      data
    );
  }

  static onClose() {
    Glean.translationsPanel.close.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onClose
    );
  }

  static onOpenFromLanguageMenu() {
    Glean.translationsPanel.openFromLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onOpenFromLanguageMenu
    );
  }

  static onChangeFromLanguage(langTag) {
    Glean.translationsPanel.changeFromLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onChangeFromLanguage,
      {
        langTag,
      }
    );
  }

  static onCloseFromLanguageMenu() {
    Glean.translationsPanel.closeFromLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onCloseFromLanguageMenu
    );
  }

  static onOpenToLanguageMenu() {
    Glean.translationsPanel.openToLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onOpenToLanguageMenu
    );
  }

  static onChangeToLanguage(langTag) {
    Glean.translationsPanel.changeToLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onChangeToLanguage,
      {
        langTag,
      }
    );
  }

  static onCloseToLanguageMenu() {
    Glean.translationsPanel.closeToLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onChangeToLanguage
    );
  }

  static onOpenSettingsMenu() {
    Glean.translationsPanel.openSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onOpenSettingsMenu
    );
  }

  static onCloseSettingsMenu() {
    Glean.translationsPanel.closeSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onCloseSettingsMenu
    );
  }

  static onCancelButton() {
    Glean.translationsPanel.cancelButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onCancelButton
    );
  }

  static onChangeSourceLanguageButton() {
    Glean.translationsPanel.changeSourceLanguageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onChangeSourceLanguageButton
    );
  }

  static onDismissErrorButton() {
    Glean.translationsPanel.dismissErrorButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onDismissErrorButton
    );
  }

  static onRestorePageButton() {
    Glean.translationsPanel.restorePageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onRestorePageButton
    );
  }

  static onTranslateButton() {
    Glean.translationsPanel.translateButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onTranslateButton
    );
  }

  static onAlwaysOfferTranslations(toggledOn) {
    Glean.translationsPanel.alwaysOfferTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onAlwaysOfferTranslations,
      {
        toggledOn,
      }
    );
  }

  static onAlwaysTranslateLanguage(langTag, toggledOn) {
    Glean.translationsPanel.alwaysTranslateLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onAlwaysTranslateLanguage,
      {
        langTag,
        toggledOn,
      }
    );
  }

  static onNeverTranslateLanguage(langTag, toggledOn) {
    Glean.translationsPanel.neverTranslateLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onNeverTranslateLanguage,
      {
        langTag,
        toggledOn,
      }
    );
  }

  static onNeverTranslateSite(toggledOn) {
    Glean.translationsPanel.neverTranslateSite.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onNeverTranslateSite,
      {
        toggledOn,
      }
    );
  }

  static onManageLanguages() {
    Glean.translationsPanel.manageLanguages.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onManageLanguages
    );
  }

  static onAboutTranslations() {
    Glean.translationsPanel.aboutTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onAboutTranslations
    );
  }

  static onLearnMoreLink() {
    Glean.translationsPanel.learnMore.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      FullPageTranslationsPanelTelemetry.onLearnMoreLink
    );
  }
}

/**
 * Telemetry functions for the SelectTranslationsPanel UI
 */
class SelectTranslationsPanelTelemetry {
  /**
   * Records a telemetry event when the SelectTranslationsPanel is opened.
   *
   * @param {object} data
   * @param {string} data.docLangTag
   * @param {boolean} data.maintainFlow
   * @param {string} data.fromLanguage
   * @param {string} data.toLanguage
   * @param {string} data.topPreferredLanguage
   */
  static onOpen(data) {
    Glean.translationsSelectTranslationsPanel.open.record({
      flow_id: data.maintainFlow
        ? TranslationsTelemetry.getOrCreateFlowId()
        : TranslationsTelemetry.createFlowId(),
      document_language: data.docLangTag,
      from_language: data.fromLanguage,
      to_language: data.toLanguage,
      top_preferred_language: data.topPreferredLanguage,
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onOpen,
      data
    );
  }

  static onClose() {
    Glean.translationsSelectTranslationsPanel.close.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onClose
    );
  }

  static onCancelButton() {
    Glean.translationsSelectTranslationsPanel.cancelButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onCancelButton
    );
  }

  static onCopyButton() {
    Glean.translationsSelectTranslationsPanel.copyButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onCopyButton
    );
  }

  static onDoneButton() {
    Glean.translationsSelectTranslationsPanel.doneButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onDoneButton
    );
  }

  static onTranslateButton() {
    Glean.translationsSelectTranslationsPanel.translateButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onTranslateButton
    );
  }

  static onTranslateFullPageButton() {
    Glean.translationsSelectTranslationsPanel.translateFullPageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onTranslateFullPageButton
    );
  }

  static onTryAgainButton() {
    Glean.translationsSelectTranslationsPanel.tryAgainButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onTryAgainButton
    );
  }

  static onChangeFromLanguage({ previousLangTag, currentLangTag, docLangTag }) {
    Glean.translationsSelectTranslationsPanel.changeFromLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      document_language: docLangTag,
      previous_language: previousLangTag,
      language: currentLangTag,
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onChangeFromLanguage,
      { previousLangTag, currentLangTag, docLangTag }
    );
  }

  static onChangeToLanguage(langTag) {
    Glean.translationsSelectTranslationsPanel.changeToLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onChangeToLanguage,
      { langTag }
    );
  }

  static onOpenSettingsMenu() {
    Glean.translationsSelectTranslationsPanel.openSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onOpenSettingsMenu
    );
  }

  static onTranslationSettings() {
    Glean.translationsSelectTranslationsPanel.translationSettings.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onTranslationSettings
    );
  }

  static onAboutTranslations() {
    Glean.translationsSelectTranslationsPanel.aboutTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onAboutTranslations
    );
  }

  static onInitializationFailureMessage() {
    Glean.translationsSelectTranslationsPanel.initializationFailureMessage.record(
      {
        flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      }
    );
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onInitializationFailureMessage
    );
  }

  /**
   * Records a telemetry event when the translation-failure message is displayed.
   *
   * @param {object} data
   * @param {string} data.fromLanguage
   * @param {string} data.toLanguage
   */
  static onTranslationFailureMessage(data) {
    Glean.translationsSelectTranslationsPanel.translationFailureMessage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      from_language: data.fromLanguage,
      to_language: data.toLanguage,
    });
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onTranslationFailureMessage,
      data
    );
  }

  /**
   * Records a telemetry event when the unsupported-language message is displayed.
   *
   * @param {object} data
   * @param {string} data.docLangTag
   * @param {string} data.detectedLanguage
   */
  static onUnsupportedLanguageMessage(data) {
    Glean.translationsSelectTranslationsPanel.unsupportedLanguageMessage.record(
      {
        flow_id: TranslationsTelemetry.getOrCreateFlowId(),
        document_language: data.docLangTag,
        detected_language: data.detectedLanguage,
      }
    );
    TranslationsTelemetry.logEventToConsole(
      SelectTranslationsPanelTelemetry.onUnsupportedLanguageMessage,
      data
    );
  }
}
