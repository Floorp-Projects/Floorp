"use strict";

extensions.registerSchemaAPI("extension", "addon_parent", context => {
  return {
    extension: {
      get lastError() {
        return context.lastError;
      },

      isAllowedIncognitoAccess() {
        return Promise.resolve(true);
      },

      isAllowedFileSchemeAccess() {
        return Promise.resolve(false);
      },
    },
  };
});

