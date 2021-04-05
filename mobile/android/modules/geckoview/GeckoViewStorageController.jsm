/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewStorageController"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PrincipalsCollector } = ChromeUtils.import(
  "resource://gre/modules/PrincipalsCollector.jsm"
);
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging(
  "GeckoViewStorageController"
);

// Keep in sync with StorageController.ClearFlags and nsIClearDataService.idl.
const ClearFlags = [
  [
    // COOKIES
    1 << 0,
    Ci.nsIClearDataService.CLEAR_COOKIES |
      Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES,
  ],
  [
    // NETWORK_CACHE
    1 << 1,
    Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
  ],
  [
    // IMAGE_CACHE
    1 << 2,
    Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
  ],
  [
    // HISTORY
    1 << 3,
    Ci.nsIClearDataService.CLEAR_HISTORY |
      Ci.nsIClearDataService.CLEAR_SESSION_HISTORY,
  ],
  [
    // DOM_STORAGES
    1 << 4,
    Ci.nsIClearDataService.CLEAR_APPCACHE |
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA |
      Ci.nsIClearDataService.CLEAR_DOM_PUSH_NOTIFICATIONS |
      Ci.nsIClearDataService.CLEAR_REPORTS,
  ],
  [
    // AUTH_SESSIONS
    1 << 5,
    Ci.nsIClearDataService.CLEAR_AUTH_TOKENS |
      Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
  ],
  [
    // PERMISSIONS
    1 << 6,
    Ci.nsIClearDataService.CLEAR_PERMISSIONS,
  ],
  [
    // SITE_SETTINGS
    1 << 7,
    Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES |
      Ci.nsIClearDataService.CLEAR_DOM_PUSH_NOTIFICATIONS |
      Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS,
  ],
  [
    // SITE_DATA
    1 << 8,
    Ci.nsIClearDataService.CLEAR_EME,
  ],
  [
    // ALL
    1 << 9,
    Ci.nsIClearDataService.CLEAR_ALL,
  ],
];

function convertFlags(aJavaFlags) {
  const flags = ClearFlags.filter(cf => {
    return cf[0] & aJavaFlags;
  }).reduce((acc, cf) => {
    return acc | cf[1];
  }, 0);
  return flags;
}

const GeckoViewStorageController = {
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:ClearData": {
        this.clearData(aData.flags, aCallback);
        break;
      }
      case "GeckoView:ClearSessionContextData": {
        this.clearSessionContextData(aData.contextId);
        break;
      }
      case "GeckoView:ClearHostData": {
        this.clearHostData(aData.host, aData.flags, aCallback);
        break;
      }
      case "GeckoView:GetAllPermissions": {
        const rawPerms = Services.perms.all;
        const permissions = rawPerms.map(p => {
          return {
            uri: Services.io.createExposableURI(p.principal.URI).displaySpec,
            principal: E10SUtils.serializePrincipal(p.principal),
            type: p.type,
            value: p.capability,
            privateMode: p.principal.privateBrowsingId != 0,
          };
        });
        aCallback.onSuccess({ permissions });
        break;
      }
      case "GeckoView:GetPermissionsByURI": {
        const uri = Services.io.newURI(aData.uri);
        const prin = Services.scriptSecurityManager.createContentPrincipal(
          uri,
          {}
        );
        const rawPerms = Services.perms.getAllForPrincipal(prin);
        const permissions = rawPerms.map(p => {
          return {
            uri: Services.io.createExposableURI(p.principal.URI).displaySpec,
            principal: E10SUtils.serializePrincipal(p.principal),
            type: p.type,
            value: p.capability,
            privateMode: p.principal.privateBrowsingId != 0,
          };
        });
        aCallback.onSuccess({ permissions });
        break;
      }
    }
  },

  async clearData(aFlags, aCallback) {
    const flags = convertFlags(aFlags);

    // storageAccessAPI permissions record every site that the user
    // interacted with and thus mirror history quite closely. It makes
    // sense to clear them when we clear history. However, since their absence
    // indicates that we can purge cookies and site data for tracking origins without
    // user interaction, we need to ensure that we only delete those permissions that
    // do not have any existing storage.
    if (flags & Ci.nsIClearDataService.CLEAR_HISTORY) {
      const principalsCollector = new PrincipalsCollector();
      const principals = await principalsCollector.getAllPrincipals();
      await new Promise(resolve => {
        Services.clearData.deleteUserInteractionForClearingHistory(
          principals,
          0,
          resolve
        );
      });
    }

    new Promise(resolve => {
      Services.clearData.deleteData(flags, resolve);
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
        resolve
      );
    }).then(resultFlags => {
      aCallback.onSuccess();
    });
  },

  clearSessionContextData(aContextId) {
    const pattern = { geckoViewSessionContextId: aContextId };
    debug`clearSessionContextData ${pattern}`;
    Services.clearData.deleteDataFromOriginAttributesPattern(pattern);
    // Call QMS explicitly to work around bug 1537882.
    Services.qms.clearStoragesForOriginAttributesPattern(
      JSON.stringify(pattern)
    );
  },
};
