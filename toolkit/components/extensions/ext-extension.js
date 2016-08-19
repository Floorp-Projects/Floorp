"use strict";

extensions.registerSchemaAPI("extension", "addon_parent", context => {
  let {extension} = context;
  return {
    extension: {
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
        // TODO(robwu): See comment about lastError in ext-runtime.js
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

