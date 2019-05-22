/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewStorageController"];

const {GeckoViewUtils} = ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const {debug, warn} = GeckoViewUtils.initLogging("GeckoViewStorageController"); // eslint-disable-line no-unused-vars

// Keep in sync with StorageController.ClearFlags and nsIClearDataService.idl.
const ClearFlags = [
  [
    // COOKIES
    (1 << 0),
    Ci.nsIClearDataService.CLEAR_COOKIES |
    Ci.nsIClearDataService.CLEAR_PLUGIN_DATA |
    Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES,
  ],
  [
    // NETWORK_CACHE
    (1 << 1),
    Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
  ],
  [
    // IMAGE_CACHE
    (1 << 2),
    Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
  ],
  [
    // HISTORY
    (1 << 3),
    Ci.nsIClearDataService.CLEAR_HISTORY |
    Ci.nsIClearDataService.CLEAR_SESSION_HISTORY |
    Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS,
  ],
  [
    // DOM_STORAGES
    (1 << 4),
    Ci.nsIClearDataService.CLEAR_APPCACHE |
    Ci.nsIClearDataService.CLEAR_DOM_QUOTA |
    Ci.nsIClearDataService.CLEAR_PUSH_NOTIFICATIONS |
    Ci.nsIClearDataService.CLEAR_REPORTS,
  ],
  [
    // AUTH_SESSIONS
    (1 << 5),
    Ci.nsIClearDataService.CLEAR_AUTH_TOKENS |
    Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
  ],
  [
    // PERMISSIONS
    (1 << 6),
    Ci.nsIClearDataService.CLEAR_PERMISSIONS,
  ],
  [
    // SITE_SETTINGS
    (1 << 7),
    Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES |
    Ci.nsIClearDataService.CLEAR_DOM_PUSH_NOTIFICATIONS |
    Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS,
  ],
  [
    // SITE_DATA
    (1 << 8),
    Ci.nsIClearDataService.CLEAR_EME,
  ],
  [
    // ALL
    (1 << 9),
    Ci.nsIClearDataService.CLEAR_ALL,
  ],
];

function convertFlags(aJavaFlags) {
  const flags = ClearFlags.filter(cf => {
    return cf[0] & aJavaFlags;
  }).reduce((acc, cf) => {
    return acc | cf[1];
  }, 0);
  return flags /* TODO: fix bug 1553454 */ & ~Ci.nsIClearDataService.CLEAR_HISTORY;
}

const GeckoViewStorageController = {
  onEvent(aEvent, aData, aCallback) {
    debug `onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:ClearData": {
        this.clearData(aData.flags, aCallback);
        break;
      }
      case "GeckoView:ClearHostData": {
        this.clearHostData(aData.host, aData.flags, aCallback);
        break;
      }
    }
  },

  clearData(aFlags, aCallback) {
    new Promise(resolve => {
      Services.clearData.deleteData(convertFlags(aFlags), resolve);
    }).then(resultFlags => {
      aCallback.onSuccess();
    });
  },

  clearHostData(aHost, aFlags, aCallback) {
    new Promise(resolve => {
      Services.clearData.deleteDataFromHost(
        aHost,
        /* isUserRequest */ true,
        convertFlags(aFlags),
        resolve);
    }).then(resultFlags => {
      aCallback.onSuccess();
    });
  },
};
