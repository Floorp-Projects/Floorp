/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/**
 * This component serves as integration between the platform and AddonManager.
 * It is responsible for initializing and shutting down the AddonManager as well
 * as passing new installs from webpages to the AddonManager.
 */

"use strict";

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

const MSG_INSTALL_ENABLED  = "WebInstallerIsInstallEnabled";
const MSG_INSTALL_ADDONS   = "WebInstallerInstallAddonsFromWebpage";
const MSG_INSTALL_CALLBACK = "WebInstallerInstallCallback";

const CHILD_SCRIPT =
  "chrome://mozapps/content/extensions/extensions-content.js";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var gSingleton = null;

function amManager() {
  Components.utils.import("resource://gre/modules/AddonManager.jsm");

  var messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                       getService(Ci.nsIChromeFrameMessageManager);

  messageManager.addMessageListener(MSG_INSTALL_ENABLED, this);
  messageManager.addMessageListener(MSG_INSTALL_ADDONS, this);
  messageManager.loadFrameScript(CHILD_SCRIPT, true, true);
}

amManager.prototype = {
  observe: function AMC_observe(aSubject, aTopic, aData) {
    let os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);

    switch (aTopic) {
    case "addons-startup":
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
  isInstallEnabled: function AMC_isInstallEnabled(aMimetype, aReferer) {
    return AddonManager.isInstallEnabled(aMimetype);
  },

  /**
   * @see amIWebInstaller.idl
   */
  installAddonsFromWebpage: function AMC_installAddonsFromWebpage(aMimetype,
                                                                  aWindow,
                                                                  aReferer, aUris,
                                                                  aHashes, aNames,
                                                                  aIcons, aCallback) {
    if (aUris.length == 0)
      return false;

    let retval = true;
    if (!AddonManager.isInstallAllowed(aMimetype, aReferer)) {
      aCallback = null;
      retval = false;
    }

    let loadGroup = null;

    try {
      loadGroup = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocumentLoader).loadGroup;
    }
    catch (e) {
    }

    let installs = [];
    function buildNextInstall() {
      if (aUris.length == 0) {
        AddonManager.installAddonsFromWebpage(aMimetype, aWindow, aReferer, installs);
        return;
      }
      let uri = aUris.shift();
      AddonManager.getInstallForURL(uri, function(aInstall) {
        function callCallback(aUri, aStatus) {
          try {
            aCallback.onInstallEnded(aUri, aStatus);
          }
          catch (e) {
            Components.utils.reportError(e);
          }
        }

        if (aInstall) {
          installs.push(aInstall);
          if (aCallback) {
            aInstall.addListener({
              onDownloadCancelled: function(aInstall) {
                callCallback(uri, USER_CANCELLED);
              },

              onDownloadFailed: function(aInstall) {
                if (aInstall.error == AddonManager.ERROR_CORRUPT_FILE)
                  callCallback(uri, CANT_READ_ARCHIVE);
                else
                  callCallback(uri, DOWNLOAD_ERROR);
              },

              onInstallFailed: function(aInstall) {
                callCallback(uri, EXECUTION_ERROR);
              },

              onInstallEnded: function(aInstall, aStatus) {
                callCallback(uri, SUCCESS);
              }
            });
          }
        }
        else if (aCallback) {
          aCallback.onInstallEnded(uri, UNSUPPORTED_TYPE);
        }
        buildNextInstall();
      }, aMimetype, aHashes.shift(), aNames.shift(), aIcons.shift(), null, loadGroup);
    }
    buildNextInstall();

    return retval;
  },

  notify: function AMC_notify(aTimer) {
    AddonManagerPrivate.backgroundUpdateCheck();
  },

  /**
   * messageManager callback function.
   *
   * Listens to requests from child processes for InstallTrigger
   * activity, and sends back callbacks.
   */
  receiveMessage: function(aMessage) {
    var payload = aMessage.json;
    var referer = Services.io.newURI(payload.referer, null, null);
    switch (aMessage.name) {
      case MSG_INSTALL_ENABLED:
        return this.isInstallEnabled(payload.mimetype, referer);

      case MSG_INSTALL_ADDONS:
        var callback = null;
        if (payload.callbackId != -1) {
          callback = {
            onInstallEnded: function ITP_callback(url, status) {
              // Doing it this way, instead of aMessage.target.messageManager,
              // ensures it works in Firefox and not only Fennec. See bug
              // 578172. TODO: Clean up this code once that bug is fixed
              var flo = aMessage.target.QueryInterface(Ci.nsIFrameLoaderOwner);
              var returnMessageManager = flo.frameLoader.messageManager;
              returnMessageManager.sendAsyncMessage(MSG_INSTALL_CALLBACK,
                { installerId: payload.installerId,
                  callbackId: payload.callbackId, url: url, status: status }
              );
            },
          };
        }
        var window = null;
        try {
          // Normal approach for single-process mode
          window = aMessage.target.contentWindow;
        } catch (e) {
          // Fallback for multiprocess (e10s) mode. Should reimplement this
          // properly with Window IDs when possible, see bug 596109.
          window = aMessage.target.ownerDocument.defaultView;
        }
        return this.installAddonsFromWebpage(payload.mimetype,
          window, referer, payload.uris, payload.hashes, payload.names,
          payload.icons, callback, payload.uris.length);
    }
  },

  classID: Components.ID("{4399533d-08d1-458c-a87a-235f74451cfa}"),
  _xpcom_factory: {
    createInstance: function(aOuter, aIid) {
      if (aOuter != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
  
      if (!gSingleton)
        gSingleton = new amManager();
      return gSingleton.QueryInterface(aIid);
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstaller,
                                         Ci.nsITimerCallback,
                                         Ci.nsIObserver])
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([amManager]);
