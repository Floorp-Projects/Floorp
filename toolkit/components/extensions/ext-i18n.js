"use strict";

extensions.registerAPI((extension, context) => {
  return {
    i18n: {
      getMessage: function(messageName, substitutions) {
        return extension.localizeMessage(messageName, substitutions);
      },
    },
  };
});
