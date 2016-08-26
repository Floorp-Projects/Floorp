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
        return PrivateBrowsingUtils.isContentWindowPrivate(context.contentWindow);
      },
    },
  };
}

extensions.registerSchemaAPI("extension", "addon_child", extensionApiFactory);
extensions.registerSchemaAPI("extension", "content_child", extensionApiFactory);
