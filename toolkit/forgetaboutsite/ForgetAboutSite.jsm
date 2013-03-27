/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

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
    // clear any and all network geolocation provider sessions
    try {
        Services.prefs.deleteBranch("geo.wifi.access_token.");
    } catch (e) {}

    PlacesUtils.history.removePagesFromHost(aDomain, true);

    // Cache
    let (cs = Cc["@mozilla.org/network/cache-service;1"].
              getService(Ci.nsICacheService)) {
      // NOTE: there is no way to clear just that domain, so we clear out
      //       everything)
      try {
        cs.evictEntries(Ci.nsICache.STORE_ANYWHERE);
      } catch (ex) {
        Cu.reportError("Exception thrown while clearing the cache: " +
          ex.toString());
      }
    }

    // Image Cache
    let (imageCache = Cc["@mozilla.org/image/tools;1"].
                      getService(Ci.imgITools).getImgCacheForDocument(null)) {
      try {
        imageCache.clearCache(false); // true=chrome, false=content
      } catch (ex) {
        Cu.reportError("Exception thrown while clearing the image cache: " +
          ex.toString());
      }
    }

    // Cookies
    let (cm = Cc["@mozilla.org/cookiemanager;1"].
              getService(Ci.nsICookieManager2)) {
      let enumerator = cm.getCookiesFromHost(aDomain);
      while (enumerator.hasMoreElements()) {
        let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
        cm.remove(cookie.host, cookie.name, cookie.path, false);
      }
    }

    // Plugin data
    const phInterface = Ci.nsIPluginHost;
    const FLAG_CLEAR_ALL = phInterface.FLAG_CLEAR_ALL;
    let (ph = Cc["@mozilla.org/plugin/host;1"].getService(phInterface)) {
      let tags = ph.getPluginTags();
      for (let i = 0; i < tags.length; i++) {
        try {
          ph.clearSiteData(tags[i], aDomain, FLAG_CLEAR_ALL, -1);
        } catch (e) {
          // Ignore errors from the plugin
        }
      }
    }

    // Downloads
    let (dm = Cc["@mozilla.org/download-manager;1"].
              getService(Ci.nsIDownloadManager)) {
      // Active downloads
      for (let enumerator of [dm.activeDownloads, dm.activePrivateDownloads]) {
        while (enumerator.hasMoreElements()) {
          let dl = enumerator.getNext().QueryInterface(Ci.nsIDownload);
          if (hasRootDomain(dl.source.host, aDomain)) {
            dl.cancel();
            dl.remove();
          }
        }
      }

      function deleteAllLike(db) {
        // NOTE: This is lossy, but we feel that it is OK to be lossy here and not
        //       invoke the cost of creating a URI for each download entry and
        //       ensure that the hostname matches.
        let stmt = db.createStatement(
          "DELETE FROM moz_downloads " +
          "WHERE source LIKE ?1 ESCAPE '/' " +
          "AND state NOT IN (?2, ?3, ?4)"
        );
        let pattern = stmt.escapeStringForLIKE(aDomain, "/");
        stmt.bindByIndex(0, "%" + pattern + "%");
        stmt.bindByIndex(1, Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING);
        stmt.bindByIndex(2, Ci.nsIDownloadManager.DOWNLOAD_PAUSED);
        stmt.bindByIndex(3, Ci.nsIDownloadManager.DOWNLOAD_QUEUED);
        try {
          stmt.execute();
        }
        finally {
          stmt.finalize();
        }
      }

      // Completed downloads
      deleteAllLike(dm.DBConnection);
      deleteAllLike(dm.privateDBConnection);

      // We want to rebuild the list if the UI is showing, so dispatch the
      // observer topic
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.notifyObservers(null, "download-manager-remove-download", null);
    }

    // Passwords
    let (lm = Cc["@mozilla.org/login-manager;1"].
              getService(Ci.nsILoginManager)) {
      // Clear all passwords for domain
      try {
        let logins = lm.getAllLogins();
        for (let i = 0; i < logins.length; i++)
          if (hasRootDomain(logins[i].hostname, aDomain))
            lm.removeLogin(logins[i]);
      }
      // XXXehsan: is there a better way to do this rather than this
      // hacky comparison?
      catch (ex if ex.message.indexOf("User canceled Master Password entry") != -1) { }

      // Clear any "do not save for this site" for this domain
      let disabledHosts = lm.getAllDisabledHosts();
      for (let i = 0; i < disabledHosts.length; i++)
        if (hasRootDomain(disabledHosts[i], aDomain))
          lm.setLoginSavingEnabled(disabledHosts, true);
    }

    // Permissions
    let (pm = Cc["@mozilla.org/permissionmanager;1"].
              getService(Ci.nsIPermissionManager)) {
      // Enumerate all of the permissions, and if one matches, remove it
      let enumerator = pm.enumerator;
      while (enumerator.hasMoreElements()) {
        let perm = enumerator.getNext().QueryInterface(Ci.nsIPermission);
        if (hasRootDomain(perm.host, aDomain))
          pm.remove(perm.host, perm.type);
      }
    }

    // Content Preferences
    let (cp = Cc["@mozilla.org/content-pref/service;1"].
              getService(Ci.nsIContentPrefService)) {
      let db = cp.DBConnection;
      // First we need to get the list of "groups" which are really just domains
      let names = [];
      let stmt = db.createStatement(
        "SELECT name " +
        "FROM groups " +
        "WHERE name LIKE ?1 ESCAPE '/'"
      );
      let pattern = stmt.escapeStringForLIKE(aDomain, "/");
      stmt.bindByIndex(0, "%" + pattern);
      try {
        while (stmt.executeStep())
          if (hasRootDomain(stmt.getString(0), aDomain))
            names.push(stmt.getString(0));
      }
      finally {
        stmt.finalize();
      }

      // Now, for each name we got back, remove all of its prefs.
      for (let i = 0; i < names.length; i++) {
        let uri = names[i];
        let enumerator = cp.getPrefs(uri, null).enumerator;
        while (enumerator.hasMoreElements()) {
          let pref = enumerator.getNext().QueryInterface(Ci.nsIProperty);
          cp.removePref(uri, pref.name, null);
        }
      }
    }

    // Offline Storages
    let (qm = Cc["@mozilla.org/dom/quota/manager;1"].
              getService(Ci.nsIQuotaManager)) {
      // delete data from both HTTP and HTTPS sites
      let caUtils = {};
      let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                         getService(Ci.mozIJSSubScriptLoader);
      scriptLoader.loadSubScript("chrome://global/content/contentAreaUtils.js",
                                 caUtils);
      let httpURI = caUtils.makeURI("http://" + aDomain);
      let httpsURI = caUtils.makeURI("https://" + aDomain);
      qm.clearStoragesForURI(httpURI);
      qm.clearStoragesForURI(httpsURI);
    }

    // Everybody else (including extensions)
    Services.obs.notifyObservers(null, "browser:purge-domain-data", aDomain);
  }
};
