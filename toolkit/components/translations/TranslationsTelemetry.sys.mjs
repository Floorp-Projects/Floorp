/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Telemetry functions for Translations actors
 */
export class TranslationsTelemetry {
  /**
   * Telemetry functions for the Translations panel.
   * @returns {Panel}
   */
  static panel() {
    return Panel;
  }

  /**
   * Records a telemetry event when full page translation fails.
   *
   * @param {string} errorMessage
   */
  static onError(errorMessage) {
    Glean.translations.errorRate.addToNumerator(1);
    Glean.translations.error.record({
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
      from_language: data.fromLanguage,
      to_language: data.toLanguage,
      auto_translate: data.autoTranslate,
    });
  }
}

/**
 * Telemetry functions for the Translations panel
 */
class Panel {
  /**
   * Records a telemetry event when the translations panel is opened.
   *
   * @param {boolean} openedFromAppMenu
   */
  static onOpen(openedFromAppMenu) {
    Glean.translationsPanel.open.record({
      opened_from: openedFromAppMenu ? "appMenu" : "translationsButton",
    });
  }
}
