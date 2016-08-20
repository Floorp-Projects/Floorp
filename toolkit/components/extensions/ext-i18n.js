"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  detectLanguage,
} = ExtensionUtils;

function i18nApiFactory(context) {
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
extensions.registerSchemaAPI("i18n", "addon_child", i18nApiFactory);
extensions.registerSchemaAPI("i18n", "content_child", i18nApiFactory);
