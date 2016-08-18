"use strict";

extensions.registerSchemaAPI("extension", (extension, context) => {
  return {
    extension: {
      getURL: function(url) {
        return extension.baseURI.resolve(url);
      },

      getViews: function(fetchProperties) {
        let result = Cu.cloneInto([], context.cloneScope);

        for (let view of extension.views) {
          if (!view.active) {
            continue;
          }
          if (fetchProperties !== null) {
            if (fetchProperties.type !== null && view.type != fetchProperties.type) {
              continue;
            }

            if (fetchProperties.windowId !== null && view.windowId != fetchProperties.windowId) {
              continue;
            }
          }

          result.push(view.contentWindow);
        }

        return result;
      },

      get lastError() {
        return context.lastError;
      },

      get inIncognitoContext() {
        return context.incognito;
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

