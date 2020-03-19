/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  globals: {
    CONTAINER_STORE: true,
    DEFAULT_STORE: true,
    EventEmitter: true,
    EventManager: true,
    InputEventManager: true,
    PRIVATE_STORE: true,
    TabBase: true,
    TabManagerBase: true,
    TabTrackerBase: true,
    WindowBase: true,
    WindowManagerBase: true,
    WindowTrackerBase: true,
    getUserContextIdForCookieStoreId: true,
    getContainerForCookieStoreId: true,
    getCookieStoreIdForContainer: true,
    getCookieStoreIdForOriginAttributes: true,
    getCookieStoreIdForTab: true,
    isContainerCookieStoreId: true,
    isDefaultCookieStoreId: true,
    isPrivateCookieStoreId: true,
    isValidCookieStoreId: true,
  },
};
