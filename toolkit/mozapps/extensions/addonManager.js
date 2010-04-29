/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

/**
 * This component serves as integration between the platform and AddonManager.
 * It is responsible for initializing and shutting down the AddonManager as well
 * as passing new installs from webpages to the AddonManager.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const PREF_EM_UPDATE_INTERVAL = "extensions.update.interval";

// The old XPInstall error codes
const EXECUTION_ERROR   = -203;
const CANT_READ_ARCHIVE = -207;
const USER_CANCELLED    = -210;
const DOWNLOAD_ERROR    = -228;
const UNSUPPORTED_TYPE  = -244;
const SUCCESS = 0;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var gSingleton = null;

function amManager() {
  Components.utils.import("resource://gre/modules/AddonManager.jsm");
}

amManager.prototype = {
  observe: function AMC_observe(subject, topic, data) {
    let os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);

    switch (topic) {
    case "profile-after-change":
      os.addObserver(this, "xpcom-shutdown", false);
      AddonManagerPrivate.startup();
      break;
    case "xpcom-shutdown":
      os.removeObserver(this, "xpcom-shutdown");
      AddonManagerPrivate.shutdown();
      break;
    }
  },

  /**
   * @see amIWebInstaller.idl
   */
  isInstallEnabled: function AMC_isInstallEnabled(mimetype, referer) {
    return AddonManager.isInstallEnabled(mimetype);
  },

  /**
   * @see amIWebInstaller.idl
   */
  installAddonsFromWebpage: function AMC_installAddonsFromWebpage(mimetype,
                                                                  window,
                                                                  referer, uris,
                                                                  hashes, names,
                                                                  icons, callback) {
    if (uris.length == 0)
      return false;

    let retval = true;
    if (!AddonManager.isInstallAllowed(mimetype, referer)) {
      callback = null;
      retval = false;
    }

    let loadgroup = null;

    try {
      loadgroup = window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocumentLoader).loadGroup;
    }
    catch (e) {
    }

    let installs = [];
    function buildNextInstall() {
      if (uris.length == 0) {
        AddonManager.installAddonsFromWebpage(mimetype, window, referer, installs);
        return;
      }
      let uri = uris.shift();
      AddonManager.getInstallForURL(uri, function(install) {
        if (install) {
          installs.push(install);
          if (callback) {
            install.addListener({
              onDownloadCancelled: function(install) {
                callback.onInstallEnded(uri, USER_CANCELLED);
              },

              onDownloadFailed: function(install, error) {
                if (error == AddonManager.ERROR_CORRUPT_FILE)
                  callback.onInstallEnded(uri, CANT_READ_ARCHIVE);
                else
                  callback.onInstallEnded(uri, DOWNLOAD_ERROR);
              },

              onInstallFailed: function(install, error) {
                callback.onInstallEnded(uri, EXECUTION_ERROR);
              },

              onInstallEnded: function(install, status) {
                callback.onInstallEnded(uri, SUCCESS);
              }
            });
          }
        }
        else if (callback) {
          callback.callback(uri, UNSUPPORTED_TYPE);
        }
        buildNextInstall();
      }, mimetype, hashes.shift(), names.shift(), icons.shift(), null, loadgroup);
    }
    buildNextInstall();

    return retval;
  },

  notify: function AMC_notify(timer) {
    AddonManagerPrivate.backgroundUpdateCheck();
  },

  classDescription: "Addons Manager",
  contractID: "@mozilla.org/addons/integration;1",
  classID: Components.ID("{4399533d-08d1-458c-a87a-235f74451cfa}"),
  _xpcom_categories: [{ category: "profile-after-change" },
                      { category: "update-timer",
                        value: "@mozilla.org/addons/integration;1," +
                               "getService,addon-background-update-timer," +
                               PREF_EM_UPDATE_INTERVAL + ",86400" }],
  _xpcom_factory: {
    createInstance: function(outer, iid) {
      if (outer != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
  
      if (!gSingleton)
        gSingleton = new amManager();
      return gSingleton.QueryInterface(iid);
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstaller,
                                         Ci.nsITimerCallback,
                                         Ci.nsIObserver])
};

function NSGetModule(compMgr, fileSpec)
  XPCOMUtils.generateModule([amManager]);
