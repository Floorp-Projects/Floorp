"use strict";

// These are defined on "global" which is used for the same scopes as the other
// ext-*.js files.
/* exported getCookieStoreIdForTab, getCookieStoreIdForContainer,
            getContainerForCookieStoreId,
            isValidCookieStoreId, isContainerCookieStoreId,
            EventManager, InputEventManager */
/* global getCookieStoreIdForTab:false, getCookieStoreIdForContainer:false,
          getContainerForCookieStoreId: false,
          isValidCookieStoreId:false, isContainerCookieStoreId:false,
          isDefaultCookieStoreId: false, isPrivateCookieStoreId:false,
          EventManager: false, InputEventManager: false */

ChromeUtils.defineModuleGetter(this, "ContextualIdentityService",
                               "resource://gre/modules/ContextualIdentityService.jsm");

Cu.importGlobalProperties(["URL"]);

ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");

global.EventEmitter = ExtensionUtils.EventEmitter;
global.EventManager = ExtensionCommon.EventManager;

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

function makeStartupPromise(event) {
  return ExtensionUtils.promiseObserved(event).then(() => {});
}

// browserPaintedPromise and browserStartupPromise are promises that
// resolve after the first browser window is painted and after browser
// windows have been restored, respectively.
global.browserPaintedPromise = makeStartupPromise("browser-delayed-startup-finished");
global.browserStartupPromise = makeStartupPromise("sessionstore-windows-restored");
