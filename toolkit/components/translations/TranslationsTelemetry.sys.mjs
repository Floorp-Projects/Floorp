/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Switch this to true to help with local debugging.
// Not intended for use in production.
const LOG_TELEMETRY_EVENTS_FOR_DEBUGGING = false;

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
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
   * Logs the telemetry event to the console if enabled by
   * the LOG_TELEMETRY_EVENTS constant.
   */
  static logEventToConsole(eventInfo) {
    if (!LOG_TELEMETRY_EVENTS_FOR_DEBUGGING) {
      return;
    }
    lazy.console.debug(
      `flowId(${TranslationsTelemetry.getOrCreateFlowId()}): ${eventInfo}`
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
    TranslationsTelemetry.logEventToConsole("onError");
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
      `onTranslate[page(${docLangTag}), preferred(${topPreferredLanguage})](${
        autoTranslate ? "auto" : "manual"
      }, ${fromLanguage}-${toLanguage})`
    );
  }

  static onRestorePage() {
    Glean.translations.restorePage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onRestorePage");
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
  static onOpen({
    viewName = null,
    autoShow = null,
    docLangTag = null,
    maintainFlow = false,
    openedFromAppMenu = false,
  }) {
    Glean.translationsPanel.open.record({
      flow_id: maintainFlow
        ? TranslationsTelemetry.getOrCreateFlowId()
        : TranslationsTelemetry.createFlowId(),
      auto_show: autoShow,
      view_name: viewName,
      document_language: docLangTag,
      opened_from: openedFromAppMenu ? "appMenu" : "translationsButton",
    });
    TranslationsTelemetry.logEventToConsole(
      `onOpen[${autoShow ? "auto" : "manual"}, ${
        openedFromAppMenu ? "appMenu" : "translationsButton"
      }, ${viewName ? viewName : "NULL"}](${docLangTag})`
    );
  }

  static onClose() {
    Glean.translationsPanel.close.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onClose");
  }

  static onOpenFromLanguageMenu() {
    Glean.translationsPanel.openFromLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onOpenFromLanguageMenu");
  }

  static onChangeFromLanguage(langTag) {
    Glean.translationsPanel.changeFromLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(`onChangeFromLanguage(${langTag})`);
  }

  static onCloseFromLanguageMenu() {
    Glean.translationsPanel.closeFromLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onCloseFromLanguageMenu");
  }

  static onOpenToLanguageMenu() {
    Glean.translationsPanel.openToLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onOpenToLanguageMenu");
  }

  static onChangeToLanguage(langTag) {
    Glean.translationsPanel.changeToLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
    });
    TranslationsTelemetry.logEventToConsole(`onChangeToLanguage(${langTag})`);
  }

  static onCloseToLanguageMenu() {
    Glean.translationsPanel.closeToLanguageMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onCloseToLanguageMenu");
  }

  static onOpenSettingsMenu() {
    Glean.translationsPanel.openSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onOpenSettingsMenu");
  }

  static onCloseSettingsMenu() {
    Glean.translationsPanel.closeSettingsMenu.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onCloseSettingsMenu");
  }

  static onCancelButton() {
    Glean.translationsPanel.cancelButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onCancelButton");
  }

  static onChangeSourceLanguageButton() {
    Glean.translationsPanel.changeSourceLanguageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onChangeSourceLanguageButton");
  }

  static onDismissErrorButton() {
    Glean.translationsPanel.dismissErrorButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onDismissErrorButton");
  }

  static onRestorePageButton() {
    Glean.translationsPanel.restorePageButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onRestorePageButton");
  }

  static onTranslateButton() {
    Glean.translationsPanel.translateButton.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onTranslateButton");
  }

  static onAlwaysOfferTranslations(toggledOn) {
    Glean.translationsPanel.alwaysOfferTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      `[${toggledOn ? "✔" : "x"}] onAlwaysOfferTranslations`
    );
  }

  static onAlwaysTranslateLanguage(langTag, toggledOn) {
    Glean.translationsPanel.alwaysTranslateLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      `[${toggledOn ? "✔" : "x"}] onAlwaysTranslateLanguage(${langTag})`
    );
  }

  static onNeverTranslateLanguage(langTag, toggledOn) {
    Glean.translationsPanel.neverTranslateLanguage.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      language: langTag,
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      `[${toggledOn ? "✔" : "x"}] onNeverTranslateLanguage(${langTag})`
    );
  }

  static onNeverTranslateSite(toggledOn) {
    Glean.translationsPanel.neverTranslateSite.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
      toggled_on: toggledOn,
    });
    TranslationsTelemetry.logEventToConsole(
      `[${toggledOn ? "✔" : "x"}] onNeverTranslateSite`
    );
  }

  static onManageLanguages() {
    Glean.translationsPanel.manageLanguages.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onManageLanguages");
  }

  static onAboutTranslations() {
    Glean.translationsPanel.aboutTranslations.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onAboutTranslations");
  }

  static onLearnMoreLink() {
    Glean.translationsPanel.learnMore.record({
      flow_id: TranslationsTelemetry.getOrCreateFlowId(),
    });
    TranslationsTelemetry.logEventToConsole("onLearnMoreLink");
  }
}
