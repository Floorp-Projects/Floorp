"use strict";

var {
  detectLanguage,
} = ExtensionUtils;

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
          return detectLanguage(text);
        },
      },
    };
  }
};
