/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// These are defined on "global" which is used for the same scopes as the other
// ext-*.js files.
/* exported getCookieStoreIdForTab, getCookieStoreIdForContainer,
            getContainerForCookieStoreId,
            isValidCookieStoreId, isContainerCookieStoreId,
            EventManager, URL */
/* global getCookieStoreIdForTab:false,
          getCookieStoreIdForContainer:false,
          getContainerForCookieStoreId: false,
          isValidCookieStoreId:false, isContainerCookieStoreId:false,
          isDefaultCookieStoreId: false, isPrivateCookieStoreId:false,
          EventManager: false */

ChromeUtils.defineESModuleGetters(this, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

var { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs"
);

var { ExtensionError } = ExtensionUtils;

global.EventEmitter = ExtensionCommon.EventEmitter;
global.EventManager = ExtensionCommon.EventManager;

/* globals DEFAULT_STORE, PRIVATE_STORE, CONTAINER_STORE */

global.DEFAULT_STORE = "firefox-default";
global.PRIVATE_STORE = "firefox-private";
global.CONTAINER_STORE = "firefox-container-";

global.getCookieStoreIdForTab = function (data, tab) {
  if (data.incognito) {
    return PRIVATE_STORE;
  }

  if (tab.userContextId) {
    return getCookieStoreIdForContainer(tab.userContextId);
  }

  return DEFAULT_STORE;
};

global.getCookieStoreIdForOriginAttributes = function (originAttributes) {
  if (originAttributes.privateBrowsingId) {
    return PRIVATE_STORE;
  }

  if (originAttributes.userContextId) {
    return getCookieStoreIdForContainer(originAttributes.userContextId);
  }

  return DEFAULT_STORE;
};

global.isPrivateCookieStoreId = function (storeId) {
  return storeId == PRIVATE_STORE;
};

global.isDefaultCookieStoreId = function (storeId) {
  return storeId == DEFAULT_STORE;
};

global.isContainerCookieStoreId = function (storeId) {
  return storeId !== null && storeId.startsWith(CONTAINER_STORE);
};

global.getCookieStoreIdForContainer = function (containerId) {
  return CONTAINER_STORE + containerId;
};

global.getContainerForCookieStoreId = function (storeId) {
  if (!isContainerCookieStoreId(storeId)) {
    return null;
  }

  let containerId = storeId.substring(CONTAINER_STORE.length);

  if (AppConstants.platform === "android") {
    return parseInt(containerId, 10);
  } // TODO: Bug 1643740, support ContextualIdentityService on Android

  if (ContextualIdentityService.getPublicIdentityFromId(containerId)) {
    return parseInt(containerId, 10);
  }

  return null;
};

global.isValidCookieStoreId = function (storeId) {
  return (
    isDefaultCookieStoreId(storeId) ||
    isPrivateCookieStoreId(storeId) ||
    isContainerCookieStoreId(storeId)
  );
};

global.getOriginAttributesPatternForCookieStoreId = function (cookieStoreId) {
  if (isDefaultCookieStoreId(cookieStoreId)) {
    return {
      userContextId: Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID,
      privateBrowsingId:
        Ci.nsIScriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID,
    };
  }
  if (isPrivateCookieStoreId(cookieStoreId)) {
    return {
      userContextId: Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID,
      privateBrowsingId: 1,
    };
  }
  if (isContainerCookieStoreId(cookieStoreId)) {
    let userContextId = getContainerForCookieStoreId(cookieStoreId);
    if (userContextId !== null) {
      return { userContextId };
    }
  }

  throw new ExtensionError("Invalid cookieStoreId");
};
