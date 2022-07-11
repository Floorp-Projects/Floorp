/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
const nsIFile = Ci.nsIFile;

var EXPORTED_SYMBOLS = ["DownloadLastDir"];

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

let nonPrivateLoadContext = Cu.createLoadContext();
let privateLoadContext = Cu.createPrivateLoadContext();

var observer = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "last-pb-context-exited":
        gDownloadLastDirFile = null;
        break;
      case "browser:purge-session-history":
        gDownloadLastDirFile = null;
        if (Services.prefs.prefHasUserValue(LAST_DIR_PREF)) {
          Services.prefs.clearUserPref(LAST_DIR_PREF);
        }
        // Ensure that purging session history causes both the session-only PB cache
        // and persistent prefs to be cleared.
        let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
          Ci.nsIContentPrefService2
        );

        cps2.removeByName(LAST_DIR_PREF, nonPrivateLoadContext);
        cps2.removeByName(LAST_DIR_PREF, privateLoadContext);
        break;
    }
  },
};

Services.obs.addObserver(observer, "last-pb-context-exited", true);
Services.obs.addObserver(observer, "browser:purge-session-history", true);

function readLastDirPref() {
  try {
    return Services.prefs.getComplexValue(LAST_DIR_PREF, nsIFile);
  } catch (e) {
    return null;
  }
}

function isContentPrefEnabled() {
  try {
    return Services.prefs.getBoolPref(SAVE_PER_SITE_PREF);
  } catch (e) {
    return true;
  }
}

var gDownloadLastDirFile = readLastDirPref();

// aForcePrivate is only used when aWindow is null.
function DownloadLastDir(aWindow, aForcePrivate) {
  let isPrivate = false;
  if (aWindow === null) {
    isPrivate = aForcePrivate || PrivateBrowsingUtils.permanentPrivateBrowsing;
  } else {
    let loadContext = aWindow.docShell.QueryInterface(Ci.nsILoadContext);
    isPrivate = loadContext.usePrivateBrowsing;
  }

  // We always use a fake load context because we may not have one (i.e.,
  // in the aWindow == null case) and because the load context associated
  // with aWindow may disappear by the time we need it. This approach is
  // safe because we only care about the private browsing state. All the
  // rest of the load context isn't of interest to the content pref service.
  this.fakeContext = isPrivate ? privateLoadContext : nonPrivateLoadContext;
}

DownloadLastDir.prototype = {
  isPrivate: function DownloadLastDir_isPrivate() {
    return this.fakeContext.usePrivateBrowsing;
  },
  // compat shims
  get file() {
    return this._getLastFile();
  },
  set file(val) {
    this.setFile(null, val);
  },
  cleanupPrivateFile() {
    gDownloadLastDirFile = null;
  },

  _getLastFile() {
    if (gDownloadLastDirFile && !gDownloadLastDirFile.exists()) {
      gDownloadLastDirFile = null;
    }

    if (this.isPrivate()) {
      if (!gDownloadLastDirFile) {
        gDownloadLastDirFile = readLastDirPref();
      }
      return gDownloadLastDirFile;
    }
    return readLastDirPref();
  },

  getFileAsync(aURI, aCallback) {
    let plainPrefFile = this._getLastFile();
    if (!aURI || !isContentPrefEnabled()) {
      Services.tm.dispatchToMainThread(() => aCallback(plainPrefFile));
      return;
    }

    let uri = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    let result = null;
    cps2.getByDomainAndName(uri, LAST_DIR_PREF, this.fakeContext, {
      handleResult: aResult => (result = aResult),
      handleCompletion(aReason) {
        let file = plainPrefFile;
        if (
          aReason == Ci.nsIContentPrefCallback2.COMPLETE_OK &&
          result instanceof Ci.nsIContentPref
        ) {
          try {
            file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            file.initWithPath(result.value);
          } catch (e) {
            file = plainPrefFile;
          }
        }
        aCallback(file);
      },
    });
  },

  setFile(aURI, aFile) {
    if (aURI && isContentPrefEnabled()) {
      let uri = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
      let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
        Ci.nsIContentPrefService2
      );
      if (aFile instanceof Ci.nsIFile) {
        cps2.set(uri, LAST_DIR_PREF, aFile.path, this.fakeContext);
      } else {
        cps2.removeByDomainAndName(uri, LAST_DIR_PREF, this.fakeContext);
      }
    }
    if (this.isPrivate()) {
      if (aFile instanceof Ci.nsIFile) {
        gDownloadLastDirFile = aFile.clone();
      } else {
        gDownloadLastDirFile = null;
      }
    } else if (aFile instanceof Ci.nsIFile) {
      Services.prefs.setComplexValue(LAST_DIR_PREF, nsIFile, aFile);
    } else if (Services.prefs.prefHasUserValue(LAST_DIR_PREF)) {
      Services.prefs.clearUserPref(LAST_DIR_PREF);
    }
  },
};
