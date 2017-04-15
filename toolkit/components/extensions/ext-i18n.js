"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
                                  "resource:///modules/translation/LanguageDetector.jsm");


this.i18n = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      i18n: {
        getMessage: function(messageName, substitutions) {
          return extension.localizeMessage(messageName, substitutions, {cloneScope: context.cloneScope});
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
