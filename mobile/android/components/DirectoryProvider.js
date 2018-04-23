/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// -----------------------------------------------------------------------
// Directory Provider for special browser folders and files
// -----------------------------------------------------------------------

const NS_APP_CACHE_PARENT_DIR = "cachePDir";
const NS_APP_DISTRIBUTION_SEARCH_DIR_LIST = "SrchPluginsDistDL";
const NS_XPCOM_CURRENT_PROCESS_DIR = "XCurProcD";
const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";
const XRE_UPDATE_ROOT_DIR     = "UpdRootD";
const ENVVAR_UPDATE_DIR       = "UPDATES_DIRECTORY";
const WEBAPPS_DIR             = "webappsDir";

const SYSTEM_DIST_PATH = `/system/${AppConstants.ANDROID_PACKAGE_NAME}/distribution`;

function DirectoryProvider() {}

DirectoryProvider.prototype = {
  classID: Components.ID("{ef0f7a87-c1ee-45a8-8d67-26f586e46a4b}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIDirectoryServiceProvider,
                                          Ci.nsIDirectoryServiceProvider2]),

  getFile: function(prop, persistent) {
    if (prop == NS_APP_CACHE_PARENT_DIR) {
      let profile = Services.dirsvc.get("ProfD", Ci.nsIFile);
      return profile;
    } else if (prop == WEBAPPS_DIR) {
      // returns the folder that should hold the webapps database file
      // For fennec we will store that in the root profile folder so that all
      // webapps can easily access it
      let profile = Services.dirsvc.get("ProfD", Ci.nsIFile);
      return profile.parent;
    } else if (prop == XRE_APP_DISTRIBUTION_DIR) {
      let distributionDirectories =  this._getDistributionDirectories();
      for (let i = 0; i < distributionDirectories.length; i++) {
        if (distributionDirectories[i].exists()) {
          return distributionDirectories[i];
        }
      }
      // Fallback: Return default data distribution directory
      return FileUtils.getDir(NS_XPCOM_CURRENT_PROCESS_DIR, ["distribution"], false);
    } else if (prop == XRE_UPDATE_ROOT_DIR) {
      let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
      if (env.exists(ENVVAR_UPDATE_DIR)) {
        let path = env.get(ENVVAR_UPDATE_DIR);
        if (path) {
          return new FileUtils.File(path);
        }
      }
      return new FileUtils.File(env.get("DOWNLOADS_DIRECTORY"));
    }

    // We are retuning null to show failure instead for throwing an error. The
    // interface is called quite a bit and throwing an error is noisy. Returning
    // null works with the way the interface is called [see bug 529077]
    return null;
  },

  /**
   * Appends the distribution-specific search engine directories to the array.
   * The distribution directory structure is as follows:
   *
   * \- distribution/
   *    \- searchplugins/
   *       |- common/
   *       \- locale/
   *          |- <locale 1>/
   *          ...
   *          \- <locale N>/
   *
   * Common engines are loaded for all locales. If there is no locale directory for
   * the current locale, there is a pref: "distribution.searchplugins.defaultLocale",
   * which specifies a default locale to use.
   */
  _appendDistroSearchDirs: function(array) {
    let distro = this.getFile(XRE_APP_DISTRIBUTION_DIR);
    if (!distro.exists())
      return;

    let searchPlugins = distro.clone();
    searchPlugins.append("searchplugins");
    if (!searchPlugins.exists())
      return;

    let commonPlugins = searchPlugins.clone();
    commonPlugins.append("common");
    if (commonPlugins.exists())
      array.push(commonPlugins);

    let localePlugins = searchPlugins.clone();
    localePlugins.append("locale");
    if (!localePlugins.exists())
      return;

    let curLocale = "";
    let reqLocales = Services.locale.getRequestedLocales();
    if (reqLocales.length > 0) {
      curLocale = reqLocales[0];
    }

    if (curLocale) {
      let curLocalePlugins = localePlugins.clone();
      curLocalePlugins.append(curLocale);
      if (curLocalePlugins.exists()) {
        array.push(curLocalePlugins);
        return;
      }
    }

    // We didn't append the locale dir - try the default one.
    try {
      let defLocale = Services.prefs.getCharPref("distribution.searchplugins.defaultLocale");
      let defLocalePlugins = localePlugins.clone();
      defLocalePlugins.append(defLocale);
      if (defLocalePlugins.exists())
        array.push(defLocalePlugins);
    } catch (e) {
    }
  },

  getFiles: function(prop) {
    if (prop != NS_APP_DISTRIBUTION_SEARCH_DIR_LIST)
      return null;

    let result = [];
    this._appendDistroSearchDirs(result);

    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsISimpleEnumerator]),
      hasMoreElements: function() {
        return result.length > 0;
      },
      getNext: function() {
        return result.shift();
      }
    };
  },

  _getDistributionDirectories: function() {
    let directories = [];

    // Send a synchronous Gecko thread event.
    EventDispatcher.instance.dispatch("Distribution:GetDirectories", null, {
      onSuccess: response =>
        directories = response.map(dir => new FileUtils.File(dir)),
    });

    return directories;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DirectoryProvider]);
