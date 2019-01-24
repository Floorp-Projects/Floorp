/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemePersister"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const PERSIST_ENABLED = true;
const PERSIST_BYPASS_CACHE = false;
const PERSIST_FILES = {
  headerURL: "lightweighttheme-header",
};

ChromeUtils.defineModuleGetter(this, "LightweightThemeManager",
  "resource://gre/modules/LightweightThemeManager.jsm");

XPCOMUtils.defineLazyGetter(this, "_prefs", () => {
  return Services.prefs.getBranch("lightweightThemes.");
});

var LightweightThemePersister = {
  get persistEnabled() {
    return PERSIST_ENABLED;
  },

  getPersistedData(aData) {
    for (let key in PERSIST_FILES) {
      try {
        if (aData[key] && _prefs.getBoolPref("persisted." + key))
          aData[key] = _getLocalImageURI(PERSIST_FILES[key]).spec
                       + "?" + aData.id + ";" + _version(aData);
      } catch (e) {}
    }
    return aData;
  },

  persistImages(aData, aCallback) {
    function onSuccess(key) {
      return function() {
        let current = LightweightThemeManager.currentThemeWithFallback;
        if (current && current.id == aData.id) {
          _prefs.setBoolPref("persisted." + key, true);
        } else {
          themeStillCurrent = false;
        }
        if (--numFilesToPersist == 0) {
          if (themeStillCurrent) {
            _prefs.setStringPref("persistedThemeID", _versionCode(aData));
          }
          if (aCallback) {
            aCallback();
          }
        }
      };
    }

    if (_prefs.getStringPref("persistedThemeID", "") == _versionCode(aData)) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    let numFilesToPersist = 0;
    let themeStillCurrent = true;
    for (let key in PERSIST_FILES) {
      _prefs.setBoolPref("persisted." + key, false);
      if (aData[key]) {
        numFilesToPersist++;
        _persistImage(aData[key], PERSIST_FILES[key], onSuccess(key));
      }
    }
  },
};

Object.freeze(LightweightThemePersister);


function _persistImage(sourceURL, localFileName, successCallback) {
  if (/^(file|resource):/.test(sourceURL))
    return;

  var targetURI = _getLocalImageURI(localFileName);
  var sourceURI = Services.io.newURI(sourceURL);

  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);

  persist.persistFlags =
    Ci.nsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
    Ci.nsIWebBrowserPersist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION |
    (PERSIST_BYPASS_CACHE ?
       Ci.nsIWebBrowserPersist.PERSIST_FLAGS_BYPASS_CACHE :
       Ci.nsIWebBrowserPersist.PERSIST_FLAGS_FROM_CACHE);

  persist.progressListener = new _persistProgressListener(successCallback);

  let sourcePrincipal = Services.scriptSecurityManager.createCodebasePrincipal(sourceURI, {});
  persist.saveURI(sourceURI, sourcePrincipal, 0,
                  null, Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
                  null, null, targetURI, null);
}

function _persistProgressListener(successCallback) {
  this.onLocationChange = function() {};
  this.onProgressChange = function() {};
  this.onStatusChange = function() {};
  this.onSecurityChange = function() {};
  this.onContentBlockingEvent = function() {};
  this.onStateChange = function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aRequest &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      // LWTs used to get their image files from the network…
      if (aRequest instanceof Ci.nsIHttpChannel &&
          aRequest.QueryInterface(Ci.nsIHttpChannel).requestSucceeded ||
          // … but static themes usually include the image data inside the
          // extension package.
          aRequest instanceof Ci.nsIChannel &&
          aRequest.originalURI.schemeIs("moz-extension") &&
          aRequest.QueryInterface(Ci.nsIChannel).contentLength > 0) {
        // success
        successCallback();
      }
      // failure
    }
  };
}

function _getLocalImageURI(localFileName) {
  let localFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  localFile.append(localFileName);
  return Services.io.newFileURI(localFile);
}

function _version(aThemeData) {
  return aThemeData.version || "";
}

function _versionCode(aThemeData) {
  return aThemeData.id + "-" + _version(aThemeData);
}
