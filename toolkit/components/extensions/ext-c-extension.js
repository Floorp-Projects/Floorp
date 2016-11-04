"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

function extensionApiFactory(context) {
  return {
    extension: {
      getURL(url) {
        return context.extension.baseURI.resolve(url);
      },

      get lastError() {
        return context.lastError;
      },

      get inIncognitoContext() {
        return context.incognito;
      },
    },
  };
}

extensions.registerSchemaAPI("extension", "addon_child", extensionApiFactory);
extensions.registerSchemaAPI("extension", "content_child", extensionApiFactory);
extensions.registerSchemaAPI("extension", "devtools_child", extensionApiFactory);
extensions.registerSchemaAPI("extension", "addon_child", context => {
  return {
    extension: {
      getViews: function(fetchProperties) {
        let result = Cu.cloneInto([], context.cloneScope);

        for (let view of context.extension.views) {
          if (!view.active) {
            continue;
          }
          if (fetchProperties !== null) {
            if (fetchProperties.type !== null && view.viewType != fetchProperties.type) {
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
    },
  };
});
