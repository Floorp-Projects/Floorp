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

import { PrivateBrowsingUtils } from "resource://gre/modules/PrivateBrowsingUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "cps2",
  "@mozilla.org/content-pref/service;1",
  "nsIContentPrefService2"
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
        let promises = [
          new Promise(resolve =>
            lazy.cps2.removeByName(LAST_DIR_PREF, nonPrivateLoadContext, {
              handleCompletion: resolve,
            })
          ),
          new Promise(resolve =>
            lazy.cps2.removeByName(LAST_DIR_PREF, privateLoadContext, {
              handleCompletion: resolve,
            })
          ),
        ];
        // This is for testing purposes.
        if (aSubject && typeof subject == "object") {
          aSubject.promise = Promise.all(promises);
        }
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

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isContentPrefEnabled",
  SAVE_PER_SITE_PREF,
  true
);

var gDownloadLastDirFile = readLastDirPref();

export class DownloadLastDir {
  // aForcePrivate is only used when aWindow is null.
  constructor(aWindow, aForcePrivate) {
    let isPrivate = false;
    if (aWindow === null) {
      isPrivate =
        aForcePrivate || PrivateBrowsingUtils.permanentPrivateBrowsing;
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

  isPrivate() {
    return this.fakeContext.usePrivateBrowsing;
  }

  // compat shims
  get file() {
    return this.#getLastFile();
  }
  set file(val) {
    this.setFile(null, val);
  }

  cleanupPrivateFile() {
    gDownloadLastDirFile = null;
  }

  #getLastFile() {
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
  }

  async getFileAsync(aURI) {
    let plainPrefFile = this.#getLastFile();
    if (!aURI || !lazy.isContentPrefEnabled) {
      return plainPrefFile;
    }

    return new Promise(resolve => {
      lazy.cps2.getByDomainAndName(
        this.#cpsGroupFromURL(aURI),
        LAST_DIR_PREF,
        this.fakeContext,
        {
          _result: null,
          handleResult(aResult) {
            this._result = aResult;
          },
          handleCompletion(aReason) {
            let file = plainPrefFile;
            if (
              aReason == Ci.nsIContentPrefCallback2.COMPLETE_OK &&
              this._result instanceof Ci.nsIContentPref
            ) {
              try {
                file = Cc["@mozilla.org/file/local;1"].createInstance(
                  Ci.nsIFile
                );
                file.initWithPath(this._result.value);
              } catch (e) {
                file = plainPrefFile;
              }
            }
            resolve(file);
          },
        }
      );
    });
  }

  setFile(aURI, aFile) {
    if (aURI && lazy.isContentPrefEnabled) {
      if (aFile instanceof Ci.nsIFile) {
        lazy.cps2.set(
          this.#cpsGroupFromURL(aURI),
          LAST_DIR_PREF,
          aFile.path,
          this.fakeContext
        );
      } else {
        lazy.cps2.removeByDomainAndName(
          this.#cpsGroupFromURL(aURI),
          LAST_DIR_PREF,
          this.fakeContext
        );
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
  }

  /**
   * Pre-processor to extract a domain name to be used with the content-prefs
   * service. This specially handles data and file URIs so that the download
   * dirs are recalled in a more consistent way:
   *  - all file:/// URIs share the same folder
   *  - data: URIs share a folder per mime-type. If a mime-type is not
   *    specified text/plain is assumed.
   *  - blob: URIs share the same folder as their origin. This is done by
   *    ContentPrefs already, so we just let the url fall-through.
   * In any other case the original URL is returned as a string and ContentPrefs
   * will do its usual parsing.
   *
   * @param {string|nsIURI|URL} url The URL to parse
   * @returns {string} the domain name to use, or the original url.
   */
  #cpsGroupFromURL(url) {
    if (typeof url == "string") {
      url = new URL(url);
    } else if (url instanceof Ci.nsIURI) {
      url = URL.fromURI(url);
    }
    if (!URL.isInstance(url)) {
      return url;
    }
    if (url.protocol == "data:") {
      return url.href.match(/^data:[^;,]*/i)[0].replace(/:$/, ":text/plain");
    }
    if (url.protocol == "file:") {
      return "file:///";
    }
    return url.href;
  }
}
