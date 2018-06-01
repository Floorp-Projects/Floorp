/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");

var EXPORTED_SYMBOLS = ["ForgetAboutSite"];

/**
 * Returns true if the string passed in is part of the root domain of the
 * current string.  For example, if this is "www.mozilla.org", and we pass in
 * "mozilla.org", this will return true.  It would return false the other way
 * around.
 */
function hasRootDomain(str, aDomain) {
  let index = str.indexOf(aDomain);
  // If aDomain is not found, we know we do not have it as a root domain.
  if (index == -1)
    return false;

  // If the strings are the same, we obviously have a match.
  if (str == aDomain)
    return true;

  // Otherwise, we have aDomain as our root domain iff the index of aDomain is
  // aDomain.length subtracted from our length and (since we do not have an
  // exact match) the character before the index is a dot or slash.
  let prevChar = str[index - 1];
  return (index == (str.length - aDomain.length)) &&
         (prevChar == "." || prevChar == "/");
}

var ForgetAboutSite = {
  async removeDataFromDomain(aDomain) {
    await PlacesUtils.history.removeByFilter({ host: "." + aDomain });

    let promises = [];

    ["http://", "https://"].forEach(scheme => {
      promises.push(new Promise(resolve => {
        Services.clearData.deleteDataFromHost(aDomain, true /* user request */,
                                              Ci.nsIClearDataService.CLEAR_ALL,
                                              resolve);
      }));
    });

    // EME
    promises.push((async function() {
      let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].
                getService(Ci.mozIGeckoMediaPluginChromeService);
      mps.forgetThisSite(aDomain, JSON.stringify({}));
    })().catch(ex => {
      throw new Error("Exception thrown while clearing Encrypted Media Extensions: " + ex);
    }));

    // Permissions
    // Enumerate all of the permissions, and if one matches, remove it
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      let perm = enumerator.getNext().QueryInterface(Ci.nsIPermission);
      promises.push(new Promise((resolve, reject) => {
        try {
          if (hasRootDomain(perm.principal.URI.host, aDomain)) {
            Services.perms.removePermission(perm);
          }
        } catch (ex) {
          // Ignore entry
        } finally {
          resolve();
        }
      }));
    }

    // Content Preferences
    promises.push((async function() {
      let cps2 = Cc["@mozilla.org/content-pref/service;1"].
                 getService(Ci.nsIContentPrefService2);
      cps2.removeBySubdomain(aDomain, null, {
        handleCompletion: (reason) => {
          // Notify other consumers, including extensions
          Services.obs.notifyObservers(null, "browser:purge-domain-data", aDomain);
          if (reason === cps2.COMPLETE_ERROR) {
            throw new Error("Exception occured while clearing content preferences");
          }
        },
        handleError() {}
      });
    })());

    // Push notifications.
    promises.push((async function() {
      var push = Cc["@mozilla.org/push/Service;1"].
                 getService(Ci.nsIPushService);
      push.clearForDomain(aDomain, status => {
        if (!Components.isSuccessCode(status)) {
          throw new Error("Exception occured while clearing push notifications: " + status);
        }
      });
    })());

    // HSTS and HPKP
    promises.push((async function() {
      let sss = Cc["@mozilla.org/ssservice;1"].
                getService(Ci.nsISiteSecurityService);
      for (let type of [Ci.nsISiteSecurityService.HEADER_HSTS,
                        Ci.nsISiteSecurityService.HEADER_HPKP]) {
        // Also remove HSTS/HPKP information for subdomains by enumerating the
        // information in the site security service.
        let enumerator = sss.enumerate(type);
        while (enumerator.hasMoreElements()) {
          let entry = enumerator.getNext();
          let hostname = entry.QueryInterface(Ci.nsISiteSecurityState).hostname;
          // If the hostname is aDomain's subdomain, we remove its state.
          if (hostname == aDomain || hostname.endsWith("." + aDomain)) {
            // This uri is used as a key to remove the state.
            let uri = NetUtil.newURI("https://" + hostname);
            sss.removeState(type, uri, 0, entry.originAttributes);
          }
        }
      }
    })().catch(ex => {
      throw new Error("Exception thrown while clearing HSTS/HPKP: " + ex);
    }));

    let ErrorCount = 0;
    for (let promise of promises) {
      try {
        await promise;
      } catch (ex) {
        Cu.reportError(ex);
        ErrorCount++;
      }
    }
    if (ErrorCount !== 0)
      throw new Error(`There were a total of ${ErrorCount} errors during removal`);
  }
};
