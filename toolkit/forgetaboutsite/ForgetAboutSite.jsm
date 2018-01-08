/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");

this.EXPORTED_SYMBOLS = ["ForgetAboutSite"];

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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.ForgetAboutSite = {
  async removeDataFromDomain(aDomain) {
    PlacesUtils.history.removePagesFromHost(aDomain, true);

    let promises = [];
    // Cache
    promises.push((async function() {
      // NOTE: there is no way to clear just that domain, so we clear out
      //       everything)
      Services.cache2.clear();
    })().catch(ex => {
      throw new Error("Exception thrown while clearing the cache: " + ex);
    }));

    // Image Cache
    promises.push((async function() {
      let imageCache = Cc["@mozilla.org/image/tools;1"].
                       getService(Ci.imgITools).getImgCacheForDocument(null);
      imageCache.clearCache(false); // true=chrome, false=content
    })().catch(ex => {
      throw new Error("Exception thrown while clearing the image cache: " + ex);
    }));

    // Cookies
    // Need to maximize the number of cookies cleaned here
    promises.push((async function() {
      let enumerator = Services.cookies.getCookiesWithOriginAttributes(JSON.stringify({}), aDomain);
      while (enumerator.hasMoreElements()) {
        let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
        Services.cookies.remove(cookie.host, cookie.name, cookie.path, false, cookie.originAttributes);
      }
    })().catch(ex => {
      throw new Error("Exception thrown while clearning cookies: " + ex);
    }));

    // EME
    promises.push((async function() {
      let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].
                getService(Ci.mozIGeckoMediaPluginChromeService);
      mps.forgetThisSite(aDomain, JSON.stringify({}));
    })().catch(ex => {
      throw new Error("Exception thrown while clearing Encrypted Media Extensions: " + ex);
    }));


    // Plugin data
    const phInterface = Ci.nsIPluginHost;
    const FLAG_CLEAR_ALL = phInterface.FLAG_CLEAR_ALL;
    let ph = Cc["@mozilla.org/plugin/host;1"].getService(phInterface);
    let tags = ph.getPluginTags();
    for (let i = 0; i < tags.length; i++) {
      promises.push(new Promise(resolve => {
        try {
          ph.clearSiteData(tags[i], aDomain, FLAG_CLEAR_ALL, -1, resolve);
        } catch (e) {
          // Ignore errors from the plugin, but resolve the promise
          // We cannot check if something is a bailout or an error
          resolve();
        }
      }));
    }

    // Downloads
    promises.push((async function() {
      let list = await Downloads.getList(Downloads.ALL);
      list.removeFinished(download => hasRootDomain(
        NetUtil.newURI(download.source.url).host, aDomain));
    })().catch(ex => {
      throw new Error("Exception in clearing Downloads: " + ex);
    }));

    // Passwords
    promises.push((async function() {
      // Clear all passwords for domain
      let logins = Services.logins.getAllLogins();
      for (let i = 0; i < logins.length; i++)
        if (hasRootDomain(logins[i].hostname, aDomain))
          Services.logins.removeLogin(logins[i]);
    })().catch(ex => {
      // XXXehsan: is there a better way to do this rather than this
      // hacky comparison?
      if (ex.message.indexOf("User canceled Master Password entry") == -1) {
        throw new Error("Exception occured in clearing passwords :" + ex);
      }
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

    // Offline Storages
    promises.push((async function() {
      // delete data from both HTTP and HTTPS sites
      let httpURI = NetUtil.newURI("http://" + aDomain);
      let httpsURI = NetUtil.newURI("https://" + aDomain);
      // Following code section has been reverted to the state before Bug 1238183,
      // but added a new argument to clearStoragesForPrincipal() for indicating
      // clear all storages under a given origin.
      let httpPrincipal = Services.scriptSecurityManager
                                   .createCodebasePrincipal(httpURI, {});
      let httpsPrincipal = Services.scriptSecurityManager
                                   .createCodebasePrincipal(httpsURI, {});
      Services.qms.clearStoragesForPrincipal(httpPrincipal, null, true);
      Services.qms.clearStoragesForPrincipal(httpsPrincipal, null, true);
    })().catch(ex => {
      throw new Error("Exception occured while clearing offline storages: " + ex);
    }));

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

    // Predictive network data - like cache, no way to clear this per
    // domain, so just trash it all
    promises.push((async function() {
      let np = Cc["@mozilla.org/network/predictor;1"].
               getService(Ci.nsINetworkPredictor);
      np.reset();
    })().catch(ex => {
      throw new Error("Exception occured while clearing predictive network data: " + ex);
    }));

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
