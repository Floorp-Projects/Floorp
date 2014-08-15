/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DirectoryLinksProvider"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const XMLHttpRequest =
  Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1", "nsIXMLHttpRequest");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});

// The filename where directory links are stored locally
const DIRECTORY_LINKS_FILE = "directoryLinks.json";

// The preference that tells whether to match the OS locale
const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";

// The preference that tells what locale the user selected
const PREF_SELECTED_LOCALE = "general.useragent.locale";

// The preference that tells where to obtain directory links
const PREF_DIRECTORY_SOURCE = "browser.newtabpage.directory.source";

// The preference that tells where to send click reports
const PREF_DIRECTORY_REPORT_CLICK_ENDPOINT = "browser.newtabpage.directory.reportClickEndPoint";

// The preference that tells if telemetry is enabled
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";

// The frecency of a directory link
const DIRECTORY_FRECENCY = 1000;

const LINK_TYPES = Object.freeze([
  "sponsored",
  "affiliate",
  "organic",
]);

/**
 * Singleton that serves as the provider of directory links.
 * Directory links are a hard-coded set of links shown if a user's link
 * inventory is empty.
 */
let DirectoryLinksProvider = {

  __linksURL: null,

  _observers: new Set(),

  // links download deferred, resolved upon download completion
  _downloadDeferred: null,

  // download default interval is 24 hours in milliseconds
  _downloadIntervalMS: 86400000,

  /**
   * A mapping from eTLD+1 to an enhanced link objects
   */
  _enhancedLinks: new Map(),

  get _observedPrefs() Object.freeze({
    linksURL: PREF_DIRECTORY_SOURCE,
    matchOSLocale: PREF_MATCH_OS_LOCALE,
    prefSelectedLocale: PREF_SELECTED_LOCALE,
  }),

  get _linksURL() {
    if (!this.__linksURL) {
      try {
        this.__linksURL = Services.prefs.getCharPref(this._observedPrefs["linksURL"]);
      }
      catch (e) {
        Cu.reportError("Error fetching directory links url from prefs: " + e);
      }
    }
    return this.__linksURL;
  },

  /**
   * Gets the currently selected locale for display.
   * @return  the selected locale or "en-US" if none is selected
   */
  get locale() {
    let matchOS;
    try {
      matchOS = Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE);
    }
    catch (e) {}

    if (matchOS) {
      return Services.locale.getLocaleComponentForUserAgent();
    }

    try {
      let locale = Services.prefs.getComplexValue(PREF_SELECTED_LOCALE,
                                                  Ci.nsIPrefLocalizedString);
      if (locale) {
        return locale.data;
      }
    }
    catch (e) {}

    try {
      return Services.prefs.getCharPref(PREF_SELECTED_LOCALE);
    }
    catch (e) {}

    return "en-US";
  },

  get linkTypes() LINK_TYPES,

  observe: function DirectoryLinksProvider_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      if (aData == this._observedPrefs["linksURL"]) {
        delete this.__linksURL;
      }
    }
    // force directory download on changes to any of the observed prefs
    this._fetchAndCacheLinksIfNecessary(true);
  },

  _addPrefsObserver: function DirectoryLinksProvider_addObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.addObserver(prefName, this, false);
    }
  },

  _removePrefsObserver: function DirectoryLinksProvider_removeObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.removeObserver(prefName, this);
    }
  },

  /**
   * Get the eTLD+1 / base domain from a url spec
   */
  _extractSite: function DirectoryLinksProvider_extractSite(url) {
    let linkURI = Services.io.newURI(url, null, null);
    try {
      return Services.eTLD.getBaseDomain(linkURI);
    }
    catch(ex) {}
    return linkURI.asciiHost;
  },

  _fetchAndCacheLinks: function DirectoryLinksProvider_fetchAndCacheLinks(uri) {
    let deferred = Promise.defer();
    let xmlHttp = new XMLHttpRequest();
    xmlHttp.overrideMimeType("application/json");

    let self = this;
    xmlHttp.onload = function(aResponse) {
      let json = this.responseText;
      if (this.status && this.status != 200) {
        json = "{}";
      }
      OS.File.writeAtomic(self._directoryFilePath, json, {tmpPath: self._directoryFilePath + ".tmp"})
        .then(() => {
          deferred.resolve();
        },
        () => {
          deferred.reject("Error writing uri data in profD.");
        });
    };

    xmlHttp.onerror = function(e) {
      deferred.reject("Fetching " + uri + " results in error code: " + e.target.status);
    };

    try {
      xmlHttp.open('POST', uri);
      xmlHttp.send(JSON.stringify({
        locale: this.locale,
        directoryCount: this._directoryCount,
      }));
    } catch (e) {
      deferred.reject("Error fetching " + uri);
      Cu.reportError(e);
    }
    return deferred.promise;
  },

  /**
   * Downloads directory links if needed
   * @return promise resolved immediately if no download needed, or upon completion
   */
  _fetchAndCacheLinksIfNecessary: function DirectoryLinksProvider_fetchAndCacheLinksIfNecessary(forceDownload=false) {
    if (this._downloadDeferred) {
      // fetching links already - just return the promise
      return this._downloadDeferred.promise;
    }

    if (forceDownload || this._needsDownload) {
      this._downloadDeferred = Promise.defer();
      this._fetchAndCacheLinks(this._linksURL).then(() => {
        // the new file was successfully downloaded and cached, so update a timestamp
        this._lastDownloadMS = Date.now();
        this._downloadDeferred.resolve();
        this._downloadDeferred = null;
        this._callObservers("onManyLinksChanged")
      },
      error => {
        this._downloadDeferred.resolve();
        this._downloadDeferred = null;
        this._callObservers("onDownloadFail");
      });
      return this._downloadDeferred.promise;
    }

    // download is not needed
    return Promise.resolve();
  },

  /**
   * @return true if download is needed, false otherwise
   */
  get _needsDownload () {
    // fail if last download occured less then 24 hours ago
    if ((Date.now() - this._lastDownloadMS) > this._downloadIntervalMS) {
      return true;
    }
    return false;
  },

  /**
   * Reads directory links file and parses its content
   * @return a promise resolved to valid list of links or [] if read or parse fails
   */
  _readDirectoryLinksFile: function DirectoryLinksProvider_readDirectoryLinksFile() {
    return OS.File.read(this._directoryFilePath).then(binaryData => {
      let output;
      try {
        let locale = this.locale;
        let json = gTextDecoder.decode(binaryData);
        let list = JSON.parse(json);
        this._listId = list.id;
        output = list[locale];
      }
      catch (e) {
        Cu.reportError(e);
      }
      return output || [];
    },
    error => {
      Cu.reportError(error);
      return [];
    });
  },

  /**
   * Report a click behavior on a link for an action
   * @param link Link object from DirectoryLinksProvider
   * @param action String of the behavior to report
   * @param tileIndex Number for the tile position of the link
   * @param pinned Boolean if the tile is pinned
   */
  reportLinkAction: function DirectoryLinksProvider_reportLinkAction(link, action, tileIndex, pinned) {
    let reportClickEndPoint;
    let telemetryEnabled = false;
    try {
      reportClickEndPoint = Services.prefs.getCharPref(PREF_DIRECTORY_REPORT_CLICK_ENDPOINT);
      telemetryEnabled = Services.prefs.getBoolPref(PREF_TELEMETRY_ENABLED);
    }
    catch (ex) {
      return;
    }

    if (!telemetryEnabled) {
      return;
    }

    // Package the data to be sent with the ping
    let ping = new XMLHttpRequest();
    let queryParams = [
      ["list", this._listId || ""],
      ["link", link.directoryIndex],
      ["action", action],
      ["tile", tileIndex],
      ["score", link.frecency],
      ["pin", +pinned],
    ].map(([key, val]) => encodeURIComponent(key) + "=" + encodeURIComponent(val));

    ping.open("GET", reportClickEndPoint + "?" + queryParams.join("&"));
    ping.send();
  },

  /**
   * Submits counts of shown directory links for each type and
   * triggers directory download if sponsored link was shown
   *
   * @param object keyed on types containing counts
   * @return download promise
   */
  reportShownCount: function DirectoryLinksProvider_reportShownCount(directoryCount) {
    // make a deep copy of directoryCount to avoid a leak
    this._directoryCount = Cu.cloneInto(directoryCount, {});
    if (directoryCount.sponsored > 0
        || directoryCount.affiliate > 0
        || directoryCount.organic > 0) {
      return this._fetchAndCacheLinksIfNecessary();
    }
    return Promise.resolve();
  },

  /**
   * Get the enhanced link object for a link (whether history or directory)
   */
  getEnhancedLink: function DirectoryLinksProvider_getEnhancedLink(link) {
    // Use the provided link if it's already enhanced
    return link.enhancedImageURI && link ||
           this._enhancedLinks.get(this._extractSite(link.url));
  },

  /**
   * Gets the current set of directory links.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function DirectoryLinksProvider_getLinks(aCallback) {
    this._readDirectoryLinksFile().then(rawLinks => {
      // Reset the cache of enhanced images for this new set of links
      this._enhancedLinks.clear();

      // all directory links have a frecency of DIRECTORY_FRECENCY
      aCallback(rawLinks.map((link, position) => {
        // Stash the enhanced image for the site
        if (link.enhancedImageURI) {
          this._enhancedLinks.set(this._extractSite(link.url), link);
        }

        link.directoryIndex = position;
        link.frecency = DIRECTORY_FRECENCY;
        link.lastVisitDate = rawLinks.length - position;
        return link;
      }));
    });
  },

  init: function DirectoryLinksProvider_init() {
    this._addPrefsObserver();
    // setup directory file path and last download timestamp
    this._directoryFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, DIRECTORY_LINKS_FILE);
    this._lastDownloadMS = 0;
    return Task.spawn(function() {
      // get the last modified time of the links file if it exists
      let doesFileExists = yield OS.File.exists(this._directoryFilePath);
      if (doesFileExists) {
        let fileInfo = yield OS.File.stat(this._directoryFilePath);
        this._lastDownloadMS = Date.parse(fileInfo.lastModificationDate);
      }
      // fetch directory on startup without force
      yield this._fetchAndCacheLinksIfNecessary();
    }.bind(this));
  },

  /**
   * Return the object to its pre-init state
   */
  reset: function DirectoryLinksProvider_reset() {
    delete this.__linksURL;
    delete this._directoryCount;
    this._removePrefsObserver();
    this._removeObservers();
  },

  addObserver: function DirectoryLinksProvider_addObserver(aObserver) {
    this._observers.add(aObserver);
  },

  removeObserver: function DirectoryLinksProvider_removeObserver(aObserver) {
    this._observers.delete(aObserver);
  },

  _callObservers: function DirectoryLinksProvider__callObservers(aMethodName, aArg) {
    for (let obs of this._observers) {
      if (typeof(obs[aMethodName]) == "function") {
        try {
          obs[aMethodName](this, aArg);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  },

  _removeObservers: function() {
    this._observers.clear();
  }
};
