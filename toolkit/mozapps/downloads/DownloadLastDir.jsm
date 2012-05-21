/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The behavior implemented by gDownloadLastDir is documented here.
 *
 * In normal browsing sessions, gDownloadLastDir uses the browser.download.lastDir
 * preference to store the last used download directory. The first time the user
 * switches into the private browsing mode, the last download directory is
 * preserved to the pref value, but if the user switches to another directory
 * during the private browsing mode, that directory is not stored in the pref,
 * and will be merely kept in memory.  When leaving the private browsing mode,
 * this in-memory value will be discarded, and the last download directory
 * will be reverted to the pref value.
 *
 * Both the pref and the in-memory value will be cleared when clearing the
 * browsing history.  This effectively changes the last download directory
 * to the default download directory on each platform.
 *
 * If passed a URI, the last used directory is also stored with that URI in the
 * content preferences database. This can be disabled by setting the pref
 * browser.download.lastDir.savePerSite to false.
 */

const LAST_DIR_PREF = "browser.download.lastDir";
const SAVE_PER_SITE_PREF = LAST_DIR_PREF + ".savePerSite";
const PBSVC_CID = "@mozilla.org/privatebrowsing;1";
const nsILocalFile = Components.interfaces.nsILocalFile;

var EXPORTED_SYMBOLS = [ "gDownloadLastDir" ];

Components.utils.import("resource://gre/modules/Services.jsm");

let pbSvc = null;
if (PBSVC_CID in Components.classes) {
  pbSvc = Components.classes[PBSVC_CID]
                    .getService(Components.interfaces.nsIPrivateBrowsingService);
}

let observer = {
  QueryInterface: function (aIID) {
    if (aIID.equals(Components.interfaces.nsIObserver) ||
        aIID.equals(Components.interfaces.nsISupports) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "private-browsing":
        if (aData == "enter")
          gDownloadLastDirFile = readLastDirPref();
        else if (aData == "exit") {
          gDownloadLastDirFile = null;
        }
        break;
      case "browser:purge-session-history":
        gDownloadLastDirFile = null;
        if (Services.prefs.prefHasUserValue(LAST_DIR_PREF))
          Services.prefs.clearUserPref(LAST_DIR_PREF);
        Services.contentPrefs.removePrefsByName(LAST_DIR_PREF);
        break;
    }
  }
};

let os = Components.classes["@mozilla.org/observer-service;1"]
                   .getService(Components.interfaces.nsIObserverService);
os.addObserver(observer, "private-browsing", true);
os.addObserver(observer, "browser:purge-session-history", true);

function readLastDirPref() {
  try {
    return Services.prefs.getComplexValue(LAST_DIR_PREF, nsILocalFile);
  }
  catch (e) {
    return null;
  }
}

function isContentPrefEnabled() {
  try {
    return Services.prefs.getBoolPref(SAVE_PER_SITE_PREF);
  } 
  catch (e) {
    return true;
  }
}

let gDownloadLastDirFile = readLastDirPref();
let gDownloadLastDir = {
  // compat shims
  get file() { return this.getFile(); },
  set file(val) { this.setFile(null, val); },
  getFile: function (aURI) {
    if (aURI && isContentPrefEnabled()) {
      let lastDir = Services.contentPrefs.getPref(aURI, LAST_DIR_PREF);
      if (lastDir) {
        var lastDirFile = Components.classes["@mozilla.org/file/local;1"]
                                    .createInstance(Components.interfaces.nsILocalFile);
        lastDirFile.initWithPath(lastDir);
        return lastDirFile;
      }
    }
    if (gDownloadLastDirFile && !gDownloadLastDirFile.exists())
      gDownloadLastDirFile = null;

    if (pbSvc && pbSvc.privateBrowsingEnabled)
      return gDownloadLastDirFile;
    else
      return readLastDirPref();
  },
  setFile: function (aURI, aFile) {
    if (aURI && isContentPrefEnabled()) {
      if (aFile instanceof Components.interfaces.nsIFile)
        Services.contentPrefs.setPref(aURI, LAST_DIR_PREF, aFile.path);
      else
        Services.contentPrefs.removePref(aURI, LAST_DIR_PREF);
    }
    if (pbSvc && pbSvc.privateBrowsingEnabled) {
      if (aFile instanceof Components.interfaces.nsIFile)
        gDownloadLastDirFile = aFile.clone();
      else
        gDownloadLastDirFile = null;
    } else {
      if (aFile instanceof Components.interfaces.nsIFile)
        Services.prefs.setComplexValue(LAST_DIR_PREF, nsILocalFile, aFile);
      else if (Services.prefs.prefHasUserValue(LAST_DIR_PREF))
        Services.prefs.clearUserPref(LAST_DIR_PREF);
    }
  }
};
