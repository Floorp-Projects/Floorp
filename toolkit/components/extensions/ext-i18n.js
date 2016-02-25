"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  detectLanguage,
} = ExtensionUtils;

extensions.registerSchemaAPI("i18n", null, (extension, context) => {
  return {
    i18n: {
      getMessage: function(messageName, substitutions) {
        return extension.localizeMessage(messageName, substitutions);
      },

      getUILanguage: function() {
        return extension.localeData.uiLocale;
      },

      detectLanguage: function(text) {
        return detectLanguage(text);
      },
    },
  };
});
