/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
  PrincipalsCollector: "resource://gre/modules/PrincipalsCollector.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceMode",
  "cookiebanners.service.mode",
  Ci.nsICookieBannerService.MODE_DISABLED
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceModePBM",
  "cookiebanners.service.mode.privateBrowsing",
  Ci.nsICookieBannerService.MODE_DISABLED
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
      Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES |
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
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
    Ci.nsIClearDataService.CLEAR_DOM_QUOTA |
      Ci.nsIClearDataService.CLEAR_DOM_PUSH_NOTIFICATIONS |
      Ci.nsIClearDataService.CLEAR_REPORTS |
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
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
      // former a part of SECURITY_SETTINGS_CLEANER
      Ci.nsIClearDataService.CLEAR_CLIENT_AUTH_REMEMBER_SERVICE,
  ],
  [
    // SITE_DATA
    1 << 8,
    Ci.nsIClearDataService.CLEAR_EME,
    // former a part of SECURITY_SETTINGS_CLEANER
    Ci.nsIClearDataService.CLEAR_HSTS,
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

export const GeckoViewStorageController = {
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
      case "GeckoView:ClearBaseDomainData": {
        this.clearBaseDomainData(aData.baseDomain, aData.flags, aCallback);
        break;
      }
      case "GeckoView:GetAllPermissions": {
        const rawPerms = Services.perms.all;
        const permissions = rawPerms.map(p => {
          return {
            uri: Services.io.createExposableURI(p.principal.URI).displaySpec,
            principal: lazy.E10SUtils.serializePrincipal(p.principal),
            perm: p.type,
            value: p.capability,
            contextId: p.principal.originAttributes.geckoViewSessionContextId,
            privateMode: p.principal.privateBrowsingId != 0,
          };
        });
        aCallback.onSuccess({ permissions });
        break;
      }
      case "GeckoView:GetPermissionsByURI": {
        const uri = Services.io.newURI(aData.uri);
        const principal = Services.scriptSecurityManager.createContentPrincipal(
          uri,
          aData.contextId
            ? {
                geckoViewSessionContextId: aData.contextId,
                privateBrowsingId: aData.privateBrowsingId,
              }
            : { privateBrowsingId: aData.privateBrowsingId }
        );
        const rawPerms = Services.perms.getAllForPrincipal(principal);
        const permissions = rawPerms.map(p => {
          return {
            uri: Services.io.createExposableURI(p.principal.URI).displaySpec,
            principal: lazy.E10SUtils.serializePrincipal(p.principal),
            perm: p.type,
            value: p.capability,
            contextId: p.principal.originAttributes.geckoViewSessionContextId,
            privateMode: p.principal.privateBrowsingId != 0,
          };
        });
        aCallback.onSuccess({ permissions });
        break;
      }
      case "GeckoView:SetPermission": {
        const principal = lazy.E10SUtils.deserializePrincipal(aData.principal);
        let key = aData.perm;
        if (key == "storage-access") {
          key = "3rdPartyFrameStorage^" + aData.thirdPartyOrigin;
        }
        if (aData.allowPermanentPrivateBrowsing) {
          Services.perms.addFromPrincipalAndPersistInPrivateBrowsing(
            principal,
            key,
            aData.newValue
          );
        } else {
          const expirePolicy = aData.privateMode
            ? Ci.nsIPermissionManager.EXPIRE_SESSION
            : Ci.nsIPermissionManager.EXPIRE_NEVER;
          Services.perms.addFromPrincipal(
            principal,
            key,
            aData.newValue,
            expirePolicy
          );
        }
        break;
      }
      case "GeckoView:SetPermissionByURI": {
        const uri = Services.io.newURI(aData.uri);
        const expirePolicy = aData.privateId
          ? Ci.nsIPermissionManager.EXPIRE_SESSION
          : Ci.nsIPermissionManager.EXPIRE_NEVER;
        const principal = Services.scriptSecurityManager.createContentPrincipal(
          uri,
          {
            geckoViewSessionContextId: aData.contextId ?? undefined,
            privateBrowsingId: aData.privateId,
          }
        );
        Services.perms.addFromPrincipal(
          principal,
          aData.perm,
          aData.newValue,
          expirePolicy
        );
        break;
      }

      case "GeckoView:SetCookieBannerModeForDomain": {
        let exceptionLabel = "SetCookieBannerModeForDomain";
        try {
          const uri = Services.io.newURI(aData.uri);
          if (aData.allowPermanentPrivateBrowsing) {
            exceptionLabel = "setDomainPrefAndPersistInPrivateBrowsing";
            Services.cookieBanners.setDomainPrefAndPersistInPrivateBrowsing(
              uri,
              aData.mode
            );
          } else {
            Services.cookieBanners.setDomainPref(
              uri,
              aData.mode,
              aData.isPrivateBrowsing
            );
          }
          aCallback.onSuccess();
        } catch (ex) {
          debug`Failed ${exceptionLabel} ${ex}`;
        }
        break;
      }

      case "GeckoView:RemoveCookieBannerModeForDomain": {
        try {
          const uri = Services.io.newURI(aData.uri);
          Services.cookieBanners.removeDomainPref(uri, aData.isPrivateBrowsing);
          aCallback.onSuccess();
        } catch (ex) {
          debug`Failed RemoveCookieBannerModeForDomain ${ex}`;
        }
        break;
      }

      case "GeckoView:GetCookieBannerModeForDomain": {
        try {
          let globalMode;
          if (aData.isPrivateBrowsing) {
            globalMode = lazy.serviceModePBM;
          } else {
            globalMode = lazy.serviceMode;
          }

          if (globalMode === Ci.nsICookieBannerService.MODE_DISABLED) {
            aCallback.onSuccess({ mode: globalMode });
            return;
          }

          const uri = Services.io.newURI(aData.uri);
          const mode = Services.cookieBanners.getDomainPref(
            uri,
            aData.isPrivateBrowsing
          );
          if (mode !== Ci.nsICookieBannerService.MODE_UNSET) {
            aCallback.onSuccess({ mode });
          } else {
            aCallback.onSuccess({ mode: globalMode });
          }
        } catch (ex) {
          aCallback.onError(`Unexpected error: ${ex}`);
          debug`Failed GetCookieBannerModeForDomain ${ex}`;
        }
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
      const principalsCollector = new lazy.PrincipalsCollector();
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

  clearBaseDomainData(aBaseDomain, aFlags, aCallback) {
    new Promise(resolve => {
      Services.clearData.deleteDataFromBaseDomain(
        aBaseDomain,
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
