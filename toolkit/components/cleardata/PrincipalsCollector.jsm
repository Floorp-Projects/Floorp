/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "serviceWorkerManager",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

let logConsole;
function log(msg) {
  if (!logConsole) {
    logConsole = console.createInstance({
      prefix: "** PrincipalsCollector.jsm",
      maxLogLevelPref: "browser.sanitizer.loglevel",
    });
  }

  logConsole.log(msg);
}

/**
 * A helper module to collect all principals that have any of the following:
 *  * cookies
 *  * quota storage (indexedDB, localStorage)
 *  * service workers
 *
 *  Note that in case of cookies, because these are not strictly associated with a
 *  full origin (including scheme), the https:// scheme will be used as a convention,
 *  so when the cookie hostname is .example.com the principal will have the origin
 *  https://example.com. Origin Attributes from cookies are copied to the principal.
 *
 *  This class is not a singleton and needs to be instantiated using the constructor
 *  before usage. The class instance will cache the last list of principals.
 *
 *  There is currently no `refresh` method, though you are free to add one.
 */
class PrincipalsCollector {
  /**
   * Creates a new PrincipalsCollector.
   */
  constructor() {
    this.principals = null;
  }

  /**
   * Checks whether the passed in principal has a scheme that is considered by the
   * PrincipalsCollector. This is used to avoid including principals for non-web
   * entities such as moz-extension.
   *
   * @param {nsIPrincipal} the principal to check
   * @returns {boolean}
   */
  static isSupportedPrincipal(principal) {
    return ["http", "https", "file"].some(scheme => principal.schemeIs(scheme));
  }

  /**
   * Fetches and collects all principals with cookies and/or site data (see module
   * description). Originally for usage in Sanitizer.jsm to compute principals to be
   * cleared on shutdown based on user settings.
   *
   * This operation might take a while to complete on big profiles.
   * DO NOT call or await this in a way that makes it block user interaction, or you
   * risk several painful seconds or possibly even minutes of lag.
   *
   * This function will cache its result and return the same list on second call,
   * even if the actual number of principals with cookies and site data changed.
   *
   * @param {Object} [optional] progress A Sanitizer.jsm progress object that will be
   *   updated to reflect the current step of fetching principals.
   * @returns {Array<nsIPrincipal>} the list of principals
   */
  async getAllPrincipals(progress = {}) {
    if (this.principals == null) {
      // Here is the list of principals with site data.
      this.principals = await this._getAllPrincipalsInternal(progress);
    }

    return this.principals;
  }

  async _getAllPrincipalsInternal(progress = {}) {
    progress.step = "principals-quota-manager";
    let principals = await new Promise(resolve => {
      Services.qms.listOrigins().callback = request => {
        progress.step = "principals-quota-manager-listOrigins";
        if (request.resultCode != Cr.NS_OK) {
          // We are probably shutting down. We don't want to propagate the
          // error, rejecting the promise.
          resolve([]);
          return;
        }

        let principalsMap = new Map();
        for (const origin of request.result) {
          let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            origin
          );
          if (PrincipalsCollector.isSupportedPrincipal(principal)) {
            principalsMap.set(principal.origin, principal);
          }
        }

        progress.step = "principals-quota-manager-completed";
        resolve(principalsMap);
      };
    }).catch(ex => {
      Cu.reportError("QuotaManagerService promise failed: " + ex);
      return [];
    });

    progress.step = "principals-service-workers";
    let serviceWorkers = lazy.serviceWorkerManager.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(
        i,
        Ci.nsIServiceWorkerRegistrationInfo
      );
      // We don't need to check the scheme. SW are just exposed to http/https URLs.
      principals.set(sw.principal.origin, sw.principal);
    }

    // Let's take the list of unique hosts+OA from cookies.
    progress.step = "principals-cookies";
    let cookies = Services.cookies.cookies;
    let hosts = new Set();
    for (let cookie of cookies) {
      hosts.add(
        cookie.rawHost +
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
      );
    }

    progress.step = "principals-host-cookie";
    hosts.forEach(host => {
      // Cookies and permissions are handled by origin/host. Doesn't matter if we
      // use http: or https: schema here.
      let principal;
      try {
        principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          "https://" + host
        );
      } catch (e) {
        log(
          `ERROR: Could not create content principal for host '${host}' ${e.message}`
        );
      }
      if (principal) {
        principals.set(principal.origin, principal);
      }
    });

    progress.step = "total-principals:" + principals.length;
    principals = Array.from(principals.values());
    return principals;
  }
}

const EXPORTED_SYMBOLS = ["PrincipalsCollector"];
