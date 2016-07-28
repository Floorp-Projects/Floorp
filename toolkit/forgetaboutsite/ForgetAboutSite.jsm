/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");

this.EXPORTED_SYMBOLS = ["ForgetAboutSite"];

/**
 * Returns true if the string passed in is part of the root domain of the
 * current string.  For example, if this is "www.mozilla.org", and we pass in
 * "mozilla.org", this will return true.  It would return false the other way
 * around.
 */
function hasRootDomain(str, aDomain)
{
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
  removeDataFromDomain: function CRH_removeDataFromDomain(aDomain)
  {
    // Get all userContextId from the ContextualIdentityService and create
    // all originAttributes.
    let oaList = [ {} ]; // init the list with the default originAttributes.

    for (let identity of ContextualIdentityService.getIdentities()) {
      oaList.push({ userContextId: identity.userContextId});
    }

    PlacesUtils.history.removePagesFromHost(aDomain, true);

    // Cache
    let cs = Cc["@mozilla.org/netwerk/cache-storage-service;1"].
             getService(Ci.nsICacheStorageService);
    // NOTE: there is no way to clear just that domain, so we clear out
    //       everything)
    try {
      cs.clear();
    } catch (ex) {
      Cu.reportError("Exception thrown while clearing the cache: " +
        ex.toString());
    }

    // Image Cache
    let imageCache = Cc["@mozilla.org/image/tools;1"].
                     getService(Ci.imgITools).getImgCacheForDocument(null);
    try {
      imageCache.clearCache(false); // true=chrome, false=content
    } catch (ex) {
      Cu.reportError("Exception thrown while clearing the image cache: " +
        ex.toString());
    }

    // Cookies
    let cm = Cc["@mozilla.org/cookiemanager;1"].
             getService(Ci.nsICookieManager2);
    let enumerator;
    for (let originAttributes of oaList) {
      enumerator = cm.getCookiesFromHost(aDomain, originAttributes);
      while (enumerator.hasMoreElements()) {
        let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
        cm.remove(cookie.host, cookie.name, cookie.path, false, cookie.originAttributes);
      }
    }

    // EME
    let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].
               getService(Ci.mozIGeckoMediaPluginChromeService);
    mps.forgetThisSite(aDomain);

    // Plugin data
    const phInterface = Ci.nsIPluginHost;
    const FLAG_CLEAR_ALL = phInterface.FLAG_CLEAR_ALL;
    let ph = Cc["@mozilla.org/plugin/host;1"].getService(phInterface);
    let tags = ph.getPluginTags();
    let promises = [];
    for (let i = 0; i < tags.length; i++) {
      let promise = new Promise(resolve => {
        let tag = tags[i];
        try {
          ph.clearSiteData(tags[i], aDomain, FLAG_CLEAR_ALL, -1, function(rv) {
            resolve();
          });
        } catch (e) {
          // Ignore errors from the plugin, but resolve the promise
          resolve();
        }
      });
      promises.push(promise);
    }

    // Downloads
    Task.spawn(function*() {
      let list = yield Downloads.getList(Downloads.ALL);
      list.removeFinished(download => hasRootDomain(
           NetUtil.newURI(download.source.url).host, aDomain));
    }).then(null, Cu.reportError);

    // Passwords
    let lm = Cc["@mozilla.org/login-manager;1"].
             getService(Ci.nsILoginManager);
    // Clear all passwords for domain
    try {
      let logins = lm.getAllLogins();
      for (let i = 0; i < logins.length; i++)
        if (hasRootDomain(logins[i].hostname, aDomain))
          lm.removeLogin(logins[i]);
    }
    // XXXehsan: is there a better way to do this rather than this
    // hacky comparison?
    catch (ex) {
      if (ex.message.indexOf("User canceled Master Password entry") == -1) {
        throw ex;
      }
    }

    // Clear any "do not save for this site" for this domain
    let disabledHosts = lm.getAllDisabledHosts();
    for (let i = 0; i < disabledHosts.length; i++)
      if (hasRootDomain(disabledHosts[i], aDomain))
        lm.setLoginSavingEnabled(disabledHosts, true);

    // Permissions
    let pm = Cc["@mozilla.org/permissionmanager;1"].
             getService(Ci.nsIPermissionManager);
    // Enumerate all of the permissions, and if one matches, remove it
    enumerator = pm.enumerator;
    while (enumerator.hasMoreElements()) {
      let perm = enumerator.getNext().QueryInterface(Ci.nsIPermission);
      try {
        if (hasRootDomain(perm.principal.URI.host, aDomain)) {
          pm.removePermission(perm);
        }
      } catch (e) {
        /* Ignore entry */
      }
    }

    // Offline Storages
    let qms = Cc["@mozilla.org/dom/quota-manager-service;1"].
              getService(Ci.nsIQuotaManagerService);
    // delete data from both HTTP and HTTPS sites
    let caUtils = {};
    let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                       getService(Ci.mozIJSSubScriptLoader);
    scriptLoader.loadSubScript("chrome://global/content/contentAreaUtils.js",
                               caUtils);
    let httpURI = caUtils.makeURI("http://" + aDomain);
    let httpsURI = caUtils.makeURI("https://" + aDomain);
    for (let originAttributes of oaList) {
      let httpPrincipal = Services.scriptSecurityManager
                                  .createCodebasePrincipal(httpURI, originAttributes);
      let httpsPrincipal = Services.scriptSecurityManager
                                   .createCodebasePrincipal(httpsURI, originAttributes);
      qms.clearStoragesForPrincipal(httpPrincipal);
      qms.clearStoragesForPrincipal(httpsPrincipal);
    }

    function onContentPrefsRemovalFinished() {
      // Everybody else (including extensions)
      Services.obs.notifyObservers(null, "browser:purge-domain-data", aDomain);
    }

    // Content Preferences
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].
               getService(Ci.nsIContentPrefService2);
    cps2.removeBySubdomain(aDomain, null, {
      handleCompletion: () => onContentPrefsRemovalFinished(),
      handleError: function() {}
    });

    // Predictive network data - like cache, no way to clear this per
    // domain, so just trash it all
    let np = Cc["@mozilla.org/network/predictor;1"].
             getService(Ci.nsINetworkPredictor);
    np.reset();

    // Push notifications.
    promises.push(new Promise(resolve => {
      var push = Cc["@mozilla.org/push/Service;1"]
                  .getService(Ci.nsIPushService);
      push.clearForDomain(aDomain, status => {
        (Components.isSuccessCode(status) ? resolve : reject)(status);
      });
    }).catch(e => {
      Cu.reportError("Exception thrown while clearing Push notifications: " +
                     e.toString());
    }));

    // HSTS and HPKP
    // TODO (bug 1290529): also remove HSTS/HPKP information for subdomains.
    // Since we can't enumerate the information in the site security service
    // (bug 1115712), we can't implement this right now.
    try {
      let sss = Cc["@mozilla.org/ssservice;1"].
                getService(Ci.nsISiteSecurityService);
      sss.removeState(Ci.nsISiteSecurityService.HEADER_HSTS, httpsURI, 0);
      sss.removeState(Ci.nsISiteSecurityService.HEADER_HPKP, httpsURI, 0);
    } catch (e) {
      Cu.reportError("Exception thrown while clearing HSTS/HPKP: " +
                     e.toString());
    }

    return Promise.all(promises);
  }
};
