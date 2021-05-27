/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  ServiceWorkerCleanUp: "resource://gre/modules/ServiceWorkerCleanUp.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "sas",
  "@mozilla.org/storage/activity-service;1",
  "nsIStorageActivityService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gFirstPartyIsolateUseSite",
  "privacy.firstparty.isolate.use_site",
  false
);

function getBaseDomainFromPartitionKey(partitionKey) {
  if (!partitionKey?.length) {
    return undefined;
  }
  if (gFirstPartyIsolateUseSite) {
    return partitionKey;
  }
  let entries = partitionKey.substr(1, partitionKey.length - 2).split(",");
  if (entries.length < 2) {
    return undefined;
  }
  return entries[1];
}

/**
 * Test if host and OriginAttributes belong to a baseDomain. Also considers
 * partitioned storage by inspecting OriginAttributes partitionKey.
 * @param options
 * @param {string} options.host - Host to compare to base domain.
 * @param {object} [options.originAttributes] - Optional origin attributes to
 * inspect for aBaseDomain. If omitted, partitionKey will not be matched.
 * @param {string} aBaseDomain - Domain to check for. Must be a valid, non-empty
 * baseDomain string.
 * @returns {boolean} Whether the host or originAttributes match the base
 * domain.
 */
function hasBaseDomain({ host, originAttributes = null }, aBaseDomain) {
  if (Services.eTLD.hasRootDomain(host, aBaseDomain)) {
    return true;
  }

  if (!originAttributes) {
    return false;
  }

  let partitionKeyBaseDomain = getBaseDomainFromPartitionKey(
    originAttributes.partitionKey
  );
  return partitionKeyBaseDomain && partitionKeyBaseDomain == aBaseDomain;
}

// Here is a list of methods cleaners may implement. These methods must return a
// Promise object.
// * deleteAll() - this method _must_ exist. When called, it deletes all the
//                 data owned by the cleaner.
// * deleteByPrincipal() -  this method _must_ exist.
// * deleteByBaseDomain() - this method _must_ exist.
// * deleteByHost() - this method is implemented only if the cleaner knows
//                    how to delete data by host + originAttributes pattern. If
//                    not implemented, deleteAll() will be used as fallback.
// * deleteByRange() - this method is implemented only if the cleaner knows how
//                    to delete data by time range. It receives 2 time range
//                    parameters: aFrom/aTo. If not implemented, deleteAll() is
//                    used as fallback.
// * deleteByLocalFiles() - this method removes data held for local files and
//                          other hostless origins. If not implemented,
//                          **no fallback is used**, as for a number of
//                          cleaners, no such data will ever exist and
//                          therefore clearing it does not make sense.
// * deleteByOriginAttributes() - this method is implemented only if the cleaner
//                                knows how to delete data by originAttributes
//                                pattern.

const CookieCleaner = {
  deleteByLocalFiles(aOriginAttributes) {
    return new Promise(aResolve => {
      Services.cookies.removeCookiesFromExactHost(
        "",
        JSON.stringify(aOriginAttributes)
      );
      aResolve();
    });
  },

  deleteByHost(aHost, aOriginAttributes) {
    return new Promise(aResolve => {
      Services.cookies.removeCookiesFromExactHost(
        aHost,
        JSON.stringify(aOriginAttributes)
      );
      aResolve();
    });
  },

  deleteByPrincipal(aPrincipal) {
    // Fall back to clearing by host and OA pattern. This will over-clear, since
    // any properties that are not explicitly set in aPrincipal.originAttributes
    // will be wildcard matched.
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteByBaseDomain(aDomain) {
    Services.cookies.cookies
      .filter(({ rawHost, originAttributes }) =>
        hasBaseDomain({ host: rawHost, originAttributes }, aDomain)
      )
      .forEach(cookie => {
        Services.cookies.removeCookiesFromExactHost(
          cookie.rawHost,
          JSON.stringify(cookie.originAttributes)
        );
      });
  },

  deleteByRange(aFrom, aTo) {
    return Services.cookies.removeAllSince(aFrom);
  },

  deleteByOriginAttributes(aOriginAttributesString) {
    return new Promise(aResolve => {
      try {
        Services.cookies.removeCookiesWithOriginAttributes(
          aOriginAttributesString
        );
      } catch (ex) {}
      aResolve();
    });
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.cookies.removeAll();
      aResolve();
    });
  },
};

const CertCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
      Ci.nsICertOverrideService
    );

    overrideService.clearValidityOverride(aHost, -1);
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteByBaseDomain(aBaseDomain) {
    let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
      Ci.nsICertOverrideService
    );
    overrideService
      .getOverrides()
      .filter(({ asciiHost }) =>
        hasBaseDomain({ host: asciiHost }, aBaseDomain)
      )
      .forEach(({ asciiHost, port }) =>
        overrideService.clearValidityOverride(asciiHost, port)
      );
  },

  async deleteAll() {
    let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
      Ci.nsICertOverrideService
    );

    overrideService.clearAllOverrides();
  },
};

const NetworkCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    Services.cache2.clearOrigin(httpPrincipal);
    Services.cache2.clearOrigin(httpsPrincipal);
  },

  deleteByPrincipal(aPrincipal) {
    return new Promise(aResolve => {
      Services.cache2.clearOrigin(aPrincipal);
      aResolve();
    });
  },

  deleteByBaseDomain(aBaseDomain) {
    // TODO: Bug 1705030
    return this.deleteByHost(aBaseDomain, {});
  },

  deleteByOriginAttributes(aOriginAttributesString) {
    return new Promise(aResolve => {
      Services.cache2.clearOriginAttributes(aOriginAttributesString);
      aResolve();
    });
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.cache2.clear();
      aResolve();
    });
  },
};

const CSSCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    ChromeUtils.clearStyleSheetCacheByPrincipal(httpPrincipal);
    ChromeUtils.clearStyleSheetCacheByPrincipal(httpsPrincipal);
  },

  async deleteByPrincipal(aPrincipal) {
    ChromeUtils.clearStyleSheetCacheByPrincipal(aPrincipal);
  },

  async deleteByBaseDomain(aBaseDomain) {
    ChromeUtils.clearStyleSheetCacheByBaseDomain(aBaseDomain);
  },

  async deleteAll() {
    ChromeUtils.clearStyleSheetCache();
  },
};

const ImageCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);

    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    imageCache.removeEntriesFromPrincipalInAllProcesses(httpPrincipal);
    imageCache.removeEntriesFromPrincipalInAllProcesses(httpsPrincipal);
  },

  async deleteByPrincipal(aPrincipal) {
    let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);
    imageCache.removeEntriesFromPrincipalInAllProcesses(aPrincipal);
  },

  async deleteByBaseDomain(aBaseDomain) {
    let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);
    imageCache.removeEntriesFromBaseDomainInAllProcesses(aBaseDomain);
  },

  deleteAll() {
    return new Promise(aResolve => {
      let imageCache = Cc["@mozilla.org/image/tools;1"]
        .getService(Ci.imgITools)
        .getImgCacheForDocument(null);
      imageCache.clearCache(false); // true=chrome, false=content
      aResolve();
    });
  },
};

const DownloadsCleaner = {
  async _deleteInternal({ hostOrBaseDomain, principal, originAttributes }) {
    originAttributes = originAttributes || principal?.originAttributes || {};

    let list = await Downloads.getList(Downloads.ALL);
    list.removeFinished(({ source }) => {
      if (
        "userContextId" in originAttributes &&
        "userContextId" in source &&
        originAttributes.userContextId != source.userContextId
      ) {
        return false;
      }
      if (
        "privateBrowsingId" in originAttributes &&
        !!originAttributes.privateBrowsingId != source.isPrivate
      ) {
        return false;
      }

      let entryURI = Services.io.newURI(source.url);
      if (hostOrBaseDomain) {
        return Services.eTLD.hasRootDomain(entryURI.host, hostOrBaseDomain);
      }
      if (principal) {
        return principal.equalsURI(entryURI);
      }
      return false;
    });
  },

  async deleteByHost(aHost, aOriginAttributes) {
    // Clearing by host also clears associated subdomains.
    return this._deleteInternal({
      hostOrBaseDomain: aHost,
      originAttributes: aOriginAttributes,
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this._deleteInternal({ principal: aPrincipal });
  },

  async deleteByBaseDomain(aBaseDomain) {
    return this._deleteInternal({ hostOrBaseDomain: aBaseDomain });
  },

  deleteByRange(aFrom, aTo) {
    // Convert microseconds back to milliseconds for date comparisons.
    let rangeBeginMs = aFrom / 1000;
    let rangeEndMs = aTo / 1000;

    return Downloads.getList(Downloads.ALL).then(aList => {
      aList.removeFinished(
        aDownload =>
          aDownload.startTime >= rangeBeginMs &&
          aDownload.startTime <= rangeEndMs
      );
    });
  },

  deleteAll() {
    return Downloads.getList(Downloads.ALL).then(aList => {
      aList.removeFinished(null);
    });
  },
};

const PasswordsCleaner = {
  deleteByHost(aHost, aOriginAttributes) {
    // Clearing by host also clears associated subdomains.
    return this._deleteInternal(aLogin =>
      Services.eTLD.hasRootDomain(aLogin.hostname, aHost)
    );
  },

  deleteByPrincipal(aPrincipal) {
    // Login origins don't contain any origin attributes.
    return this._deleteInternal(
      aLogin => aLogin.origin == aPrincipal.originNoSuffix
    );
  },

  deleteByBaseDomain(aBaseDomain) {
    return this._deleteInternal(aLogin =>
      Services.eTLD.hasRootDomain(aLogin.hostname, aBaseDomain)
    );
  },

  deleteAll() {
    return this._deleteInternal(() => true);
  },

  async _deleteInternal(aCb) {
    try {
      let logins = Services.logins.getAllLogins();
      for (let login of logins) {
        if (aCb(login)) {
          Services.logins.removeLogin(login);
        }
      }
    } catch (ex) {
      // XXXehsan: is there a better way to do this rather than this
      // hacky comparison?
      if (
        !ex.message.includes("User canceled Master Password entry") &&
        ex.result != Cr.NS_ERROR_NOT_IMPLEMENTED
      ) {
        throw new Error("Exception occured in clearing passwords: " + ex);
      }
    }
  },
};

const MediaDevicesCleaner = {
  async deleteByRange(aFrom, aTo) {
    let mediaMgr = Cc["@mozilla.org/mediaManagerService;1"].getService(
      Ci.nsIMediaManagerService
    );
    mediaMgr.sanitizeDeviceIds(aFrom);
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteAll();
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteAll();
  },

  async deleteAll() {
    let mediaMgr = Cc["@mozilla.org/mediaManagerService;1"].getService(
      Ci.nsIMediaManagerService
    );
    mediaMgr.sanitizeDeviceIds(null);
  },
};

const QuotaCleaner = {
  deleteByPrincipal(aPrincipal) {
    // localStorage: The legacy LocalStorage implementation that will
    // eventually be removed depends on this observer notification to clear by
    // principal.
    Services.obs.notifyObservers(
      null,
      "extension:purge-localStorage",
      aPrincipal.host
    );

    // Clear sessionStorage
    Services.obs.notifyObservers(
      null,
      "browser:purge-sessionStorage",
      aPrincipal.host
    );

    // ServiceWorkers: they must be removed before cleaning QuotaManager.
    return ServiceWorkerCleanUp.removeFromPrincipal(aPrincipal)
      .then(
        _ => /* exceptionThrown = */ false,
        _ => /* exceptionThrown = */ true
      )
      .then(exceptionThrown => {
        // QuotaManager: In the event of a failure, we call reject to propagate
        // the error upwards.
        return new Promise((aResolve, aReject) => {
          let req = Services.qms.clearStoragesForPrincipal(aPrincipal);
          req.callback = () => {
            if (exceptionThrown || req.resultCode != Cr.NS_OK) {
              aReject({ message: "Delete by principal failed" });
            } else {
              aResolve();
            }
          };
        });
      });
  },

  deleteByBaseDomain(aBaseDomain) {
    // TODO: Bug 1705036
    return this.deleteByHost(aBaseDomain, {});
  },

  deleteByHost(aHost, aOriginAttributes) {
    // XXX: The aOriginAttributes is expected to always be empty({}). Maybe have
    // a debug assertion here to ensure that?

    // localStorage: The legacy LocalStorage implementation that will
    // eventually be removed depends on this observer notification to clear by
    // host.  Some other subsystems like Reporting headers depend on this too.
    Services.obs.notifyObservers(null, "extension:purge-localStorage", aHost);

    // Clear sessionStorage
    Services.obs.notifyObservers(null, "browser:purge-sessionStorage", aHost);

    // ServiceWorkers: they must be removed before cleaning QuotaManager.
    return ServiceWorkerCleanUp.removeFromHost(aHost)
      .then(
        _ => /* exceptionThrown = */ false,
        _ => /* exceptionThrown = */ true
      )
      .then(exceptionThrown => {
        // QuotaManager: In the event of a failure, we call reject to propagate
        // the error upwards.

        // deleteByHost has the semantics that "foo.example.com" should be
        // wiped if we are provided an aHost of "example.com".
        return new Promise((aResolve, aReject) => {
          Services.qms.listOrigins().callback = aRequest => {
            if (aRequest.resultCode != Cr.NS_OK) {
              aReject({ message: "Delete by host failed" });
              return;
            }

            let promises = [];
            for (const origin of aRequest.result) {
              let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                origin
              );
              let host;
              try {
                host = principal.host;
              } catch (e) {
                // There is no host for the given principal.
                continue;
              }

              if (Services.eTLD.hasRootDomain(host, aHost)) {
                promises.push(
                  new Promise((aResolve, aReject) => {
                    let clearRequest = Services.qms.clearStoragesForPrincipal(
                      principal
                    );
                    clearRequest.callback = () => {
                      if (clearRequest.resultCode == Cr.NS_OK) {
                        aResolve();
                      } else {
                        aReject({ message: "Delete by host failed" });
                      }
                    };
                  })
                );
              }
            }
            Promise.all(promises).then(exceptionThrown ? aReject : aResolve);
          };
        });
      });
  },

  deleteByRange(aFrom, aTo) {
    let principals = sas
      .getActiveOrigins(aFrom, aTo)
      .QueryInterface(Ci.nsIArray);

    let promises = [];
    for (let i = 0; i < principals.length; ++i) {
      let principal = principals.queryElementAt(i, Ci.nsIPrincipal);

      if (
        !principal.schemeIs("http") &&
        !principal.schemeIs("https") &&
        !principal.schemeIs("file")
      ) {
        continue;
      }

      promises.push(this.deleteByPrincipal(principal));
    }

    return Promise.all(promises);
  },

  deleteByOriginAttributes(aOriginAttributesString) {
    // The legacy LocalStorage implementation that will eventually be removed.
    // And it should've been cleared while notifying observers with
    // clear-origin-attributes-data.

    return ServiceWorkerCleanUp.removeFromOriginAttributes(
      aOriginAttributesString
    )
      .then(
        _ => /* exceptionThrown = */ false,
        _ => /* exceptionThrown = */ true
      )
      .then(exceptionThrown => {
        // QuotaManager: In the event of a failure, we call reject to propagate
        // the error upwards.
        return new Promise((aResolve, aReject) => {
          let req = Services.qms.clearStoragesForOriginAttributesPattern(
            aOriginAttributesString
          );
          req.callback = () => {
            if (req.resultCode == Cr.NS_OK) {
              aResolve();
            } else {
              aReject({ message: "Delete by origin attributes failed" });
            }
          };
        });
      });
  },

  deleteAll() {
    // localStorage
    Services.obs.notifyObservers(null, "extension:purge-localStorage");

    // sessionStorage
    Services.obs.notifyObservers(null, "browser:purge-sessionStorage");

    // ServiceWorkers
    return ServiceWorkerCleanUp.removeAll()
      .then(
        _ => /* exceptionThrown = */ false,
        _ => /* exceptionThrown = */ true
      )
      .then(exceptionThrown => {
        // QuotaManager: In the event of a failure, we call reject to propagate
        // the error upwards.
        return new Promise((aResolve, aReject) => {
          Services.qms.getUsage(aRequest => {
            if (aRequest.resultCode != Cr.NS_OK) {
              aReject({ message: "Delete all failed" });
              return;
            }

            let promises = [];
            for (let item of aRequest.result) {
              let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                item.origin
              );
              if (
                principal.schemeIs("http") ||
                principal.schemeIs("https") ||
                principal.schemeIs("file")
              ) {
                promises.push(
                  new Promise((aResolve, aReject) => {
                    let req = Services.qms.clearStoragesForPrincipal(principal);
                    req.callback = () => {
                      if (req.resultCode == Cr.NS_OK) {
                        aResolve();
                      } else {
                        aReject({ message: "Delete all failed" });
                      }
                    };
                  })
                );
              }
            }

            Promise.all(promises).then(exceptionThrown ? aReject : aResolve);
          });
        });
      });
  },
};

const PredictorNetworkCleaner = {
  async deleteAll() {
    // Predictive network data - like cache, no way to clear this per
    // domain, so just trash it all
    let np = Cc["@mozilla.org/network/predictor;1"].getService(
      Ci.nsINetworkPredictor
    );
    np.reset();
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteAll();
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteAll();
  },
};

const PushNotificationsCleaner = {
  deleteByHost(aHost, aOriginAttributes) {
    if (!Services.prefs.getBoolPref("dom.push.enabled", false)) {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let push = Cc["@mozilla.org/push/Service;1"].getService(
        Ci.nsIPushService
      );
      push.clearForDomain(aHost, aStatus => {
        if (!Components.isSuccessCode(aStatus)) {
          aReject();
        } else {
          aResolve();
        }
      });
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteByBaseDomain(aBaseDomain) {
    // TODO: Bug 1709623
    return this.deleteByHost(aBaseDomain, {});
  },

  deleteAll() {
    if (!Services.prefs.getBoolPref("dom.push.enabled", false)) {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let push = Cc["@mozilla.org/push/Service;1"].getService(
        Ci.nsIPushService
      );
      push.clearForDomain("*", aStatus => {
        if (!Components.isSuccessCode(aStatus)) {
          aReject();
        } else {
          aResolve();
        }
      });
    });
  },
};

const StorageAccessCleaner = {
  // This is a special function to implement deleteUserInteractionForClearingHistory.
  async deleteExceptPrincipals(aPrincipalsWithStorage, aFrom) {
    // We compare by base domain in order to simulate the behavior
    // from purging, Consider a scenario where the user is logged
    // into sub.example.com but the cookies are on example.com. In this
    // case, we will remove the user interaction for sub.example.com
    // because its principal does not match the one with storage.
    let baseDomainsWithStorage = new Set();
    for (let principal of aPrincipalsWithStorage) {
      baseDomainsWithStorage.add(principal.baseDomain);
    }

    for (let perm of Services.perms.getAllByTypeSince(
      "storageAccessAPI",
      aFrom
    )) {
      if (!baseDomainsWithStorage.has(perm.principal.baseDomain)) {
        Services.perms.removePermission(perm);
      }
    }
  },

  async deleteByPrincipal(aPrincipal) {
    return Services.perms.removeFromPrincipal(aPrincipal, "storageAccessAPI");
  },

  _deleteInternal(filter) {
    Services.perms.all
      .filter(({ type }) => type == "storageAccessAPI")
      .filter(filter)
      .forEach(perm => {
        try {
          Services.perms.removePermission(perm);
        } catch (ex) {
          Cu.reportError(ex);
        }
      });
  },

  async deleteByHost(aHost, aOriginAttributes) {
    // Clearing by host also clears associated subdomains.
    this._deleteInternal(({ principal }) => {
      let toBeRemoved = false;
      try {
        toBeRemoved = Services.eTLD.hasRootDomain(principal.host, aHost);
      } catch (ex) {}
      return toBeRemoved;
    });
  },

  async deleteByBaseDomain(aBaseDomain) {
    this._deleteInternal(
      ({ principal }) => principal.baseDomain == aBaseDomain
    );
  },

  async deleteByRange(aFrom, aTo) {
    Services.perms.removeByTypeSince("storageAccessAPI", aFrom / 1000);
  },

  async deleteAll() {
    Services.perms.removeByType("storageAccessAPI");
  },
};

const HistoryCleaner = {
  deleteByHost(aHost, aOriginAttributes) {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return PlacesUtils.history.removeByFilter({ host: "." + aHost });
  },

  deleteByPrincipal(aPrincipal) {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return PlacesUtils.history.removeByFilter({ host: aPrincipal.host });
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteByHost(aBaseDomain, {});
  },

  deleteByRange(aFrom, aTo) {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return PlacesUtils.history.removeVisitsByFilter({
      beginDate: new Date(aFrom / 1000),
      endDate: new Date(aTo / 1000),
    });
  },

  deleteAll() {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return PlacesUtils.history.clear();
  },
};

const SessionHistoryCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Session storage and history also clear subdomains of aHost.
    Services.obs.notifyObservers(null, "browser:purge-sessionStorage", aHost);
    Services.obs.notifyObservers(
      null,
      "browser:purge-session-history-for-domain",
      aHost
    );
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteByHost(aBaseDomain, {});
  },

  async deleteByRange(aFrom, aTo) {
    Services.obs.notifyObservers(
      null,
      "browser:purge-session-history",
      String(aFrom)
    );
  },

  async deleteAll() {
    Services.obs.notifyObservers(null, "browser:purge-session-history");
  },
};

const AuthTokensCleaner = {
  deleteByPrincipal(aPrincipal) {
    return this.deleteAll();
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteAll();
  },

  async deleteAll() {
    let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
      Ci.nsISecretDecoderRing
    );
    sdr.logoutAndTeardown();
  },
};

const AuthCacheCleaner = {
  deleteByPrincipal(aPrincipal) {
    return this.deleteAll();
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteAll();
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.obs.notifyObservers(null, "net:clear-active-logins");
      aResolve();
    });
  },
};

const PermissionsCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    for (let perm of Services.perms.all) {
      let toBeRemoved;
      try {
        toBeRemoved = Services.eTLD.hasRootDomain(perm.principal.host, aHost);
      } catch (ex) {
        continue;
      }

      if (!toBeRemoved && perm.type.startsWith("3rdPartyStorage^")) {
        let parts = perm.type.split("^");
        let uri;
        try {
          uri = Services.io.newURI(parts[1]);
        } catch (ex) {
          continue;
        }

        toBeRemoved = Services.eTLD.hasRootDomain(uri.host, aHost);
      }

      if (!toBeRemoved) {
        continue;
      }

      try {
        Services.perms.removePermission(perm);
      } catch (ex) {
        // Ignore entry
      }
    }
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteByBaseDomain(aBaseDomain) {
    // TODO: Bug 1709624
    return this.deleteByHost(aBaseDomain, {});
  },

  async deleteByRange(aFrom, aTo) {
    Services.perms.removeAllSince(aFrom / 1000);
  },

  async deleteByOriginAttributes(aOriginAttributesString) {
    Services.perms.removePermissionsWithAttributes(aOriginAttributesString);
  },

  async deleteAll() {
    Services.perms.removeAll();
  },
};

const PreferencesCleaner = {
  deleteByHost(aHost, aOriginAttributes) {
    // Also clears subdomains of aHost.
    return new Promise((aResolve, aReject) => {
      let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
        Ci.nsIContentPrefService2
      );
      cps2.removeBySubdomain(aHost, null, {
        handleCompletion: aReason => {
          if (aReason === cps2.COMPLETE_ERROR) {
            aReject();
          } else {
            aResolve();
          }
        },
        handleError() {},
      });
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteByHost(aBaseDomain, {});
  },

  async deleteByRange(aFrom, aTo) {
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    cps2.removeAllDomainsSince(aFrom / 1000, null);
  },

  async deleteAll() {
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    cps2.removeAllDomains(null);
  },
};

const SecuritySettingsCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let sss = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );
    // Also remove HSTS information for subdomains by enumerating
    // the information in the site security service.
    for (let entry of sss.enumerate(Ci.nsISiteSecurityService.HEADER_HSTS)) {
      let hostname = entry.hostname;
      if (Services.eTLD.hasRootDomain(hostname, aHost)) {
        // This uri is used as a key to reset the state.
        let uri = Services.io.newURI("https://" + hostname);
        sss.resetState(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          uri,
          0,
          entry.originAttributes
        );
      }
    }
    let cars = Cc[
      "@mozilla.org/security/clientAuthRememberService;1"
    ].getService(Ci.nsIClientAuthRememberService);

    cars.deleteDecisionsByHost(aHost, aOriginAttributes);
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteByBaseDomain(aDomain) {
    let sss = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );

    // Remove HSTS information by enumerating entries of the site security
    // service.
    Array.from(sss.enumerate(Ci.nsISiteSecurityService.HEADER_HSTS))
      .filter(({ hostname, originAttributes }) =>
        hasBaseDomain({ host: hostname, originAttributes }, aDomain)
      )
      .forEach(({ hostname, originAttributes }) => {
        // This uri is used as a key to reset the state.
        let uri = Services.io.newURI("https://" + hostname);
        sss.resetState(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          uri,
          0,
          originAttributes
        );
      });

    let cars = Cc[
      "@mozilla.org/security/clientAuthRememberService;1"
    ].getService(Ci.nsIClientAuthRememberService);

    cars
      .getDecisions()
      .filter(({ asciiHost }) => hasBaseDomain({ host: asciiHost }, aDomain))
      .forEach(({ entryKey }) => cars.forgetRememberedDecision(entryKey));
  },

  async deleteAll() {
    // Clear site security settings - no support for ranges in this
    // interface either, so we clearAll().
    let sss = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );
    sss.clearAll();
    let cars = Cc[
      "@mozilla.org/security/clientAuthRememberService;1"
    ].getService(Ci.nsIClientAuthRememberService);
    cars.clearRememberedDecisions();
  },
};

const EMECleaner = {
  deleteByHost(aHost, aOriginAttributes) {
    return new Promise(aResolve => {
      let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(
        Ci.mozIGeckoMediaPluginChromeService
      );
      mps.forgetThisSite(aHost, JSON.stringify(aOriginAttributes));
      aResolve();
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteByBaseDomain(aBaseDomain) {
    // TODO: Bug 1705034
    return this.deleteByHost(aBaseDomain, {});
  },

  deleteAll() {
    // Not implemented.
    return Promise.resolve();
  },
};

const ReportsCleaner = {
  deleteByHost(aHost, aOriginAttributes) {
    // Also clears subdomains of aHost.
    return new Promise(aResolve => {
      Services.obs.notifyObservers(null, "reporting:purge-host", aHost);
      aResolve();
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteByHost(aBaseDomain, {});
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.obs.notifyObservers(null, "reporting:purge-all");
      aResolve();
    });
  },
};

const ContentBlockingCleaner = {
  deleteAll() {
    return TrackingDBService.clearAll();
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteAll();
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteAll();
  },

  deleteByRange(aFrom, aTo) {
    return TrackingDBService.clearSince(aFrom);
  },
};

/**
 * The about:home startup cache, if it exists, might contain information
 * about where the user has been, or what they've downloaded.
 */
const AboutHomeStartupCacheCleaner = {
  deleteByPrincipal(aPrincipal) {
    return this.deleteAll();
  },

  deleteByBaseDomain(aBaseDomain) {
    return this.deleteAll();
  },

  deleteAll() {
    // This cleaner only makes sense on Firefox desktop, which is the only
    // application that uses the about:home startup cache.
    if (!AppConstants.MOZ_BUILD_APP == "browser") {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let lci = Services.loadContextInfo.default;
      let storage = Services.cache2.diskCacheStorage(lci);
      let uri = Services.io.newURI("about:home");
      try {
        storage.asyncDoomURI(uri, "", {
          onCacheEntryDoomed(aResult) {
            if (
              Components.isSuccessCode(aResult) ||
              aResult == Cr.NS_ERROR_NOT_AVAILABLE
            ) {
              aResolve();
            } else {
              aReject({
                message: "asyncDoomURI for about:home failed",
              });
            }
          },
        });
      } catch (e) {
        aReject({
          message: "Failed to doom about:home startup cache entry",
        });
      }
    });
  },
};

// Here the map of Flags-Cleaners.
const FLAGS_MAP = [
  {
    flag: Ci.nsIClearDataService.CLEAR_CERT_EXCEPTIONS,
    cleaners: [CertCleaner],
  },

  { flag: Ci.nsIClearDataService.CLEAR_COOKIES, cleaners: [CookieCleaner] },

  {
    flag: Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
    cleaners: [NetworkCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
    cleaners: [ImageCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CSS_CACHE,
    cleaners: [CSSCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_DOWNLOADS,
    cleaners: [DownloadsCleaner, AboutHomeStartupCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_PASSWORDS,
    cleaners: [PasswordsCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES,
    cleaners: [MediaDevicesCleaner],
  },

  { flag: Ci.nsIClearDataService.CLEAR_DOM_QUOTA, cleaners: [QuotaCleaner] },

  {
    flag: Ci.nsIClearDataService.CLEAR_PREDICTOR_NETWORK_DATA,
    cleaners: [PredictorNetworkCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_DOM_PUSH_NOTIFICATIONS,
    cleaners: [PushNotificationsCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_HISTORY,
    cleaners: [HistoryCleaner, AboutHomeStartupCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_SESSION_HISTORY,
    cleaners: [SessionHistoryCleaner, AboutHomeStartupCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_AUTH_TOKENS,
    cleaners: [AuthTokensCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
    cleaners: [AuthCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_PERMISSIONS,
    cleaners: [PermissionsCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
    cleaners: [PreferencesCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS,
    cleaners: [SecuritySettingsCleaner],
  },

  { flag: Ci.nsIClearDataService.CLEAR_EME, cleaners: [EMECleaner] },

  { flag: Ci.nsIClearDataService.CLEAR_REPORTS, cleaners: [ReportsCleaner] },

  {
    flag: Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS,
    cleaners: [StorageAccessCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CONTENT_BLOCKING_RECORDS,
    cleaners: [ContentBlockingCleaner],
  },
];

this.ClearDataService = function() {
  this._initialize();
};

ClearDataService.prototype = Object.freeze({
  classID: Components.ID("{0c06583d-7dd8-4293-b1a5-912205f779aa}"),
  QueryInterface: ChromeUtils.generateQI(["nsIClearDataService"]),
  _xpcom_factory: ComponentUtils.generateSingletonFactory(ClearDataService),

  _initialize() {
    // Let's start all the service we need to cleanup data.

    // This is mainly needed for GeckoView that doesn't start QMS on startup
    // time.
    if (!Services.qms) {
      Cu.reportError("Failed initializiation of QuotaManagerService.");
    }
  },

  deleteDataFromLocalFiles(aIsUserRequest, aFlags, aCallback) {
    if (!aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      // Some of the 'Cleaners' do not support clearing data for
      // local files. Ignore those.
      if (aCleaner.deleteByLocalFiles) {
        // A generic originAttributes dictionary.
        return aCleaner.deleteByLocalFiles({});
      }
      return Promise.resolve();
    });
  },

  deleteDataFromHost(aHost, aIsUserRequest, aFlags, aCallback) {
    if (!aHost || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      // Some of the 'Cleaners' do not support to delete by principal. Let's
      // use deleteAll() as fallback.
      if (aCleaner.deleteByHost) {
        // A generic originAttributes dictionary.
        return aCleaner.deleteByHost(aHost, {});
      }
      // The user wants to delete data. Let's remove as much as we can.
      if (aIsUserRequest) {
        return aCleaner.deleteAll();
      }
      // We don't want to delete more than what is strictly required.
      return Promise.resolve();
    });
  },

  deleteDataFromBaseDomain(aDomainOrHost, aIsUserRequest, aFlags, aCallback) {
    if (!aDomainOrHost || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }
    // We may throw here if aDomainOrHost can't be converted to a base domain.
    let baseDomain = Services.eTLD.getBaseDomainFromHost(aDomainOrHost);

    return this._deleteInternal(aFlags, aCallback, aCleaner =>
      aCleaner.deleteByBaseDomain(baseDomain, aIsUserRequest)
    );
  },

  deleteDataFromPrincipal(aPrincipal, aIsUserRequest, aFlags, aCallback) {
    if (!aPrincipal || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner =>
      aCleaner.deleteByPrincipal(aPrincipal, aIsUserRequest)
    );
  },

  deleteDataInTimeRange(aFrom, aTo, aIsUserRequest, aFlags, aCallback) {
    if (aFrom > aTo || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      // Some of the 'Cleaners' do not support to delete by range. Let's use
      // deleteAll() as fallback.
      if (aCleaner.deleteByRange) {
        return aCleaner.deleteByRange(aFrom, aTo);
      }
      // The user wants to delete data. Let's remove as much as we can.
      if (aIsUserRequest) {
        return aCleaner.deleteAll();
      }
      // We don't want to delete more than what is strictly required.
      return Promise.resolve();
    });
  },

  deleteData(aFlags, aCallback) {
    if (!aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      return aCleaner.deleteAll();
    });
  },

  deleteDataFromOriginAttributesPattern(aPattern, aCallback) {
    if (!aPattern) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    let patternString = JSON.stringify(aPattern);
    // XXXtt remove clear-origin-attributes-data entirely
    Services.obs.notifyObservers(
      null,
      "clear-origin-attributes-data",
      patternString
    );

    if (!aCallback) {
      aCallback = {
        onDataDeleted: () => {},
      };
    }
    return this._deleteInternal(
      Ci.nsIClearDataService.CLEAR_ALL,
      aCallback,
      aCleaner => {
        if (aCleaner.deleteByOriginAttributes) {
          return aCleaner.deleteByOriginAttributes(patternString);
        }

        // We don't want to delete more than what is strictly required.
        return Promise.resolve();
      }
    );
  },

  deleteUserInteractionForClearingHistory(
    aPrincipalsWithStorage,
    aFrom,
    aCallback
  ) {
    if (!aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    StorageAccessCleaner.deleteExceptPrincipals(
      aPrincipalsWithStorage,
      aFrom
    ).then(() => {
      aCallback.onDataDeleted(0);
    });
    return Cr.NS_OK;
  },

  // This internal method uses aFlags against FLAGS_MAP in order to retrieve a
  // list of 'Cleaners'. For each of them, the aHelper callback retrieves a
  // promise object. All these promise objects are resolved before calling
  // onDataDeleted.
  _deleteInternal(aFlags, aCallback, aHelper) {
    let resultFlags = 0;
    let promises = FLAGS_MAP.filter(c => aFlags & c.flag).map(c => {
      return Promise.all(
        c.cleaners.map(cleaner => {
          return aHelper(cleaner).catch(e => {
            Cu.reportError(e);
            resultFlags |= c.flag;
          });
        })
      );
      // Let's collect the failure in resultFlags.
    });
    Promise.all(promises).then(() => {
      aCallback.onDataDeleted(resultFlags);
    });
    return Cr.NS_OK;
  },
});

var EXPORTED_SYMBOLS = ["ClearDataService"];
