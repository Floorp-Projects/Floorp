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
   * Telemetry functions for the Translations panel.
   *
   * @returns {Panel}
   */
  static panel() {
    return Panel;
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
   * Records a telemetry event when a translation request is sent.
   *
   * @param {object} data
   * @param {string} data.docLangTag
   * @param {string} data.fromLanguage
   * @param {string} data.toLanguage
   * @param {string} data.topPreferredLanguage
   * @param {boolean} data.autoTranslate
   */
  static onTranslate(data) {
    const {
      docLangTag,
      fromLanguage,
      toLanguage,
      autoTranslate,
      topPreferredLanguage,
    } = data;
    Glean.translations.requestsCount.add(1);
    Glean.translations.translationRequest.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      from_language: fromLanguage,
      to_language: toLanguage,
      auto_translate: autoTranslate,
      document_language: docLangTag,
      top_preferred_language: topPreferredLanguage,
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
 * Telemetry functions for the Translations panel
 */
class Panel {
  /**
   * Records a telemetry event when the translations panel is opened.
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
    TranslationsTelemetry.logEventToConsole(Panel.onOpen, data);
  }

  static onClose() {
    Glean.translationsPanel.close.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onClose);
  }

  static onOpenFromLanguageMenu() {
    Glean.translationsPanel.openFromLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onOpenFromLanguageMenu);
  }

  static onChangeFromLanguage(langTag) {
    Glean.translationsPanel.changeFromLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(Panel.onChangeFromLanguage, {
      langTag,
    });
  }

  static onCloseFromLanguageMenu() {
    Glean.translationsPanel.closeFromLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onCloseFromLanguageMenu);
  }

  static onOpenToLanguageMenu() {
    Glean.translationsPanel.openToLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onOpenToLanguageMenu);
  }

  static onChangeToLanguage(langTag) {
    Glean.translationsPanel.changeToLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(Panel.onChangeToLanguage, {
      langTag,
    });
  }

  static onCloseToLanguageMenu() {
    Glean.translationsPanel.closeToLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onChangeToLanguage);
  }

  static onOpenSettingsMenu() {
    Glean.translationsPanel.openSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onOpenSettingsMenu);
  }

  static onCloseSettingsMenu() {
    Glean.translationsPanel.closeSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onCloseSettingsMenu);
  }

  static onCancelButton() {
    Glean.translationsPanel.cancelButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onCancelButton);
  }

  static onChangeSourceLanguageButton() {
    Glean.translationsPanel.changeSourceLanguageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onChangeSourceLanguageButton);
  }

  static onDismissErrorButton() {
    Glean.translationsPanel.dismissErrorButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onDismissErrorButton);
  }

  static onRestorePageButton() {
    Glean.translationsPanel.restorePageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onRestorePageButton);
  }

  static onTranslateButton() {
    Glean.translationsPanel.translateButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onTranslateButton);
  }

  static onAlwaysOfferTranslations(toggledOn) {
    Glean.translationsPanel.alwaysOfferTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(Panel.onAlwaysOfferTranslations, {
      toggledOn,
    });
  }

  static onAlwaysTranslateLanguage(langTag, toggledOn) {
    Glean.translationsPanel.alwaysTranslateLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(Panel.onAlwaysTranslateLanguage, {
      langTag,
      toggledOn,
    });
  }

  static onNeverTranslateLanguage(langTag, toggledOn) {
    Glean.translationsPanel.neverTranslateLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(Panel.onNeverTranslateLanguage, {
      langTag,
      toggledOn,
    });
  }

  static onNeverTranslateSite(toggledOn) {
    Glean.translationsPanel.neverTranslateSite.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(Panel.onNeverTranslateSite, {
      toggledOn,
    });
  }

  static onManageLanguages() {
    Glean.translationsPanel.manageLanguages.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onManageLanguages);
  }

  static onAboutTranslations() {
    Glean.translationsPanel.aboutTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onAboutTranslations);
  }

  static onLearnMoreLink() {
    Glean.translationsPanel.learnMore.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole(Panel.onLearnMoreLink);
  }
}
