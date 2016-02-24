"use strict";

extensions.registerSchemaAPI("i18n", null, (extension, context) => {
  return {
    i18n: {
      getMessage: function(messageName, substitutions) {
        return extension.localizeMessage(messageName, substitutions);
      },

      getUILanguage: function() {
        return extension.localeData.uiLocale;
      },
    },
  };
});
