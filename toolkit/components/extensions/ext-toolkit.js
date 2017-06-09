"use strict";

// These are defined on "global" which is used for the same scopes as the other
// ext-*.js files.
/* exported getCookieStoreIdForTab, getCookieStoreIdForContainer,
            getContainerForCookieStoreId,
            isValidCookieStoreId, isContainerCookieStoreId,
            SingletonEventManager */
/* global getCookieStoreIdForTab:false, getCookieStoreIdForContainer:false,
          getContainerForCookieStoreId: false,
          isValidCookieStoreId:false, isContainerCookieStoreId:false,
          isDefaultCookieStoreId: false, isPrivateCookieStoreId:false,
          SingletonEventManager: false */

XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");

Cu.import("resource://gre/modules/ExtensionCommon.jsm");

global.SingletonEventManager = ExtensionCommon.SingletonEventManager;

/* globals DEFAULT_STORE, PRIVATE_STORE, CONTAINER_STORE */

global.DEFAULT_STORE = "firefox-default";
global.PRIVATE_STORE = "firefox-private";
global.CONTAINER_STORE = "firefox-container-";

global.getCookieStoreIdForTab = function(data, tab) {
  if (data.incognito) {
    return PRIVATE_STORE;
  }

  if (tab.userContextId) {
    return getCookieStoreIdForContainer(tab.userContextId);
  }

  return DEFAULT_STORE;
};

global.isPrivateCookieStoreId = function(storeId) {
  return storeId == PRIVATE_STORE;
};

global.isDefaultCookieStoreId = function(storeId) {
  return storeId == DEFAULT_STORE;
};

global.isContainerCookieStoreId = function(storeId) {
  return storeId !== null && storeId.startsWith(CONTAINER_STORE);
};

global.getCookieStoreIdForContainer = function(containerId) {
  return CONTAINER_STORE + containerId;
};

global.getContainerForCookieStoreId = function(storeId) {
  if (!isContainerCookieStoreId(storeId)) {
    return null;
  }

  let containerId = storeId.substring(CONTAINER_STORE.length);
  if (ContextualIdentityService.getPublicIdentityFromId(containerId)) {
    return parseInt(containerId, 10);
  }

  return null;
};

global.isValidCookieStoreId = function(storeId) {
  return isDefaultCookieStoreId(storeId) ||
         isPrivateCookieStoreId(storeId) ||
         isContainerCookieStoreId(storeId);
};

extensions.registerModules({
  manifest: {
    schema: "chrome://extensions/content/schemas/extension_types.json",
    scopes: [],
  },
  alarms: {
    url: "chrome://extensions/content/ext-alarms.js",
    schema: "chrome://extensions/content/schemas/alarms.json",
    scopes: ["addon_parent"],
    paths: [
      ["alarms"],
    ],
  },
  backgroundPage: {
    url: "chrome://extensions/content/ext-backgroundPage.js",
    scopes: ["addon_parent"],
    manifest: ["background"],
  },
  contextualIdentities: {
    url: "chrome://extensions/content/ext-contextualIdentities.js",
    schema: "chrome://extensions/content/schemas/contextual_identities.json",
    scopes: ["addon_parent"],
    paths: [
      ["contextualIdentities"],
    ],
  },
  cookies: {
    url: "chrome://extensions/content/ext-cookies.js",
    schema: "chrome://extensions/content/schemas/cookies.json",
    scopes: ["addon_parent"],
    paths: [
      ["cookies"],
    ],
  },
  downloads: {
    url: "chrome://extensions/content/ext-downloads.js",
    schema: "chrome://extensions/content/schemas/downloads.json",
    scopes: ["addon_parent"],
    paths: [
      ["downloads"],
    ],
  },
  extension: {
    url: "chrome://extensions/content/ext-extension.js",
    schema: "chrome://extensions/content/schemas/extension.json",
    scopes: ["addon_parent"],
    paths: [
      ["extension"],
    ],
  },
  geolocation: {
    url: "chrome://extensions/content/ext-geolocation.js",
    events: ["startup"],
  },
  i18n: {
    url: "chrome://extensions/content/ext-i18n.js",
    schema: "chrome://extensions/content/schemas/i18n.json",
    scopes: ["addon_parent", "content_child", "devtools_child"],
    paths: [
      ["i18n"],
    ],
  },
  idle: {
    url: "chrome://extensions/content/ext-idle.js",
    schema: "chrome://extensions/content/schemas/idle.json",
    scopes: ["addon_parent"],
    paths: [
      ["idle"],
    ],
  },
  management: {
    url: "chrome://extensions/content/ext-management.js",
    schema: "chrome://extensions/content/schemas/management.json",
    scopes: ["addon_parent"],
    paths: [
      ["management"],
    ],
  },
  notifications: {
    url: "chrome://extensions/content/ext-notifications.js",
    schema: "chrome://extensions/content/schemas/notifications.json",
    scopes: ["addon_parent"],
    paths: [
      ["notifications"],
    ],
  },
  permissions: {
    url: "chrome://extensions/content/ext-permissions.js",
    schema: "chrome://extensions/content/schemas/permissions.json",
    scopes: ["addon_parent"],
    paths: [
      ["permissions"],
    ],
  },
  privacy: {
    url: "chrome://extensions/content/ext-privacy.js",
    schema: "chrome://extensions/content/schemas/privacy.json",
    scopes: ["addon_parent"],
    paths: [
      ["privacy"],
    ],
  },
  protocolHandlers: {
    url: "chrome://extensions/content/ext-protocolHandlers.js",
    schema: "chrome://extensions/content/schemas/extension_protocol_handlers.json",
    scopes: ["addon_parent"],
    manifest: ["protocol_handlers"],
  },
  proxy: {
    url: "chrome://extensions/content/ext-proxy.js",
    schema: "chrome://extensions/content/schemas/proxy.json",
    scopes: ["addon_parent"],
    paths: [
      ["proxy"],
    ],
  },
  runtime: {
    url: "chrome://extensions/content/ext-runtime.js",
    schema: "chrome://extensions/content/schemas/runtime.json",
    scopes: ["addon_parent", "content_parent", "devtools_parent"],
    paths: [
      ["runtime"],
    ],
  },
  storage: {
    url: "chrome://extensions/content/ext-storage.js",
    schema: "chrome://extensions/content/schemas/storage.json",
    scopes: ["addon_parent", "content_parent", "devtools_parent"],
    paths: [
      ["storage"],
    ],
  },
  test: {
    schema: "chrome://extensions/content/schemas/test.json",
    scopes: [],
  },
  theme: {
    url: "chrome://extensions/content/ext-theme.js",
    schema: "chrome://extensions/content/schemas/theme.json",
    scopes: ["addon_parent"],
    manifest: ["theme"],
    paths: [
      ["theme"],
    ],
  },
  topSites: {
    url: "chrome://extensions/content/ext-topSites.js",
    schema: "chrome://extensions/content/schemas/top_sites.json",
    scopes: ["addon_parent"],
    paths: [
      ["topSites"],
    ],
  },
  webNavigation: {
    url: "chrome://extensions/content/ext-webNavigation.js",
    schema: "chrome://extensions/content/schemas/web_navigation.json",
    scopes: ["addon_parent"],
    paths: [
      ["webNavigation"],
    ],
  },
  webRequest: {
    url: "chrome://extensions/content/ext-webRequest.js",
    schema: "chrome://extensions/content/schemas/web_request.json",
    scopes: ["addon_parent"],
    paths: [
      ["webRequest"],
    ],
  },
});

if (AppConstants.MOZ_BUILD_APP === "browser") {
  extensions.registerModules({
    identity: {
      schema: "chrome://extensions/content/schemas/identity.json",
      scopes: ["addon_parent"],
    },
  });
}
