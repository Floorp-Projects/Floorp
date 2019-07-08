"use strict";

this.extension = class extends ExtensionAPI {
  getAPI(context) {
    return {
      extension: {
        get lastError() {
          return context.lastError;
        },

        isAllowedIncognitoAccess() {
          return context.privateBrowsingAllowed;
        },

        isAllowedFileSchemeAccess() {
          return false;
        },
      },
    };
  }
};
