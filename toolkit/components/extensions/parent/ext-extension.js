"use strict";

this.extension = class extends ExtensionAPI {
  getAPI(context) {
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
  }
};

