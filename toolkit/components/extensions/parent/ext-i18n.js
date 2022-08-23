/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "LanguageDetector",
  "resource://gre/modules/translation/LanguageDetector.jsm"
);

this.i18n = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    return {
      i18n: {
        getMessage: function(messageName, substitutions) {
          return extension.localizeMessage(messageName, substitutions, {
            cloneScope: context.cloneScope,
          });
        },

        getAcceptLanguages: function() {
          let result = extension.localeData.acceptLanguages;
          return Promise.resolve(result);
        },

        getUILanguage: function() {
          return extension.localeData.uiLocale;
        },

        detectLanguage: function(text) {
          return LanguageDetector.detectLanguage(text).then(result => ({
            isReliable: result.confident,
            languages: result.languages.map(lang => {
              return {
                language: lang.languageCode,
                percentage: lang.percent,
              };
            }),
          }));
        },
      },
    };
  }
};
