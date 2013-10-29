/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
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
    PlacesUtils.history.removePagesFromHost(aDomain, true);

    // Cache
    let (cs = Cc["@mozilla.org/netwerk/cache-storage-service;1"].
              getService(Ci.nsICacheStorageService)) {
      // NOTE: there is no way to clear just that domain, so we clear out
      //       everything)
      try {
        cs.clear();
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
    let useJSTransfer = false;
    try {
      // This method throws an exception if the old Download Manager is disabled.
      Services.downloads.activeDownloadCount;
    } catch (ex) {
      useJSTransfer = true;
    }

    if (useJSTransfer) {
      Task.spawn(function() {
        let list = yield Downloads.getList(Downloads.ALL);
        list.removeFinished(download => hasRootDomain(
             NetUtil.newURI(download.source.url).host, aDomain));
      }).then(null, Cu.reportError);
    }
    else {
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

    function onContentPrefsRemovalFinished() {
      // Everybody else (including extensions)
      Services.obs.notifyObservers(null, "browser:purge-domain-data", aDomain);
    }

    // Content Preferences
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].
               getService(Ci.nsIContentPrefService2);
    cps2.removeBySubdomain(aDomain, null, {
      handleCompletion: function() onContentPrefsRemovalFinished(),
      handleError: function() {}
    });

    // Predictive network data - like cache, no way to clear this per
    // domain, so just trash it all
    let ns = Cc["@mozilla.org/network/seer;1"].
             getService(Ci.nsINetworkSeer);
    ns.reset();
  }
};
