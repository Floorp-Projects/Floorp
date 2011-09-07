/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Download Manager Utility Code.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Geoff Lankow <geoff@darktrojan.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 */

const LAST_DIR_PREF = "browser.download.lastDir";
const PBSVC_CID = "@mozilla.org/privatebrowsing;1";
const nsILocalFile = Components.interfaces.nsILocalFile;

var EXPORTED_SYMBOLS = [ "gDownloadLastDir" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Dict.jsm");

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

let gDownloadLastDirFile = readLastDirPref();
let gDownloadLastDir = {
  // compat shims
  get file() { return this.getFile(); },
  set file(val) { this.setFile(null, val); },
  getFile: function (aURI) {
    if (aURI) {
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
    if (aURI) {
      Services.contentPrefs.setPref(aURI, LAST_DIR_PREF, aFile.path);
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
