/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component serves as integration between the platform and AddonManager.
 * It is responsible for initializing and shutting down the AddonManager as well
 * as passing new installs from webpages to the AddonManager.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// The old XPInstall error codes
const EXECUTION_ERROR   = -203;
const CANT_READ_ARCHIVE = -207;
const USER_CANCELLED    = -210;
const DOWNLOAD_ERROR    = -228;
const UNSUPPORTED_TYPE  = -244;
const SUCCESS           = 0;

const MSG_INSTALL_ENABLED  = "WebInstallerIsInstallEnabled";
const MSG_INSTALL_ADDONS   = "WebInstallerInstallAddonsFromWebpage";
const MSG_INSTALL_CALLBACK = "WebInstallerInstallCallback";

const MSG_PROMISE_REQUEST  = "WebAPIPromiseRequest";
const MSG_PROMISE_RESULT   = "WebAPIPromiseResult";

const CHILD_SCRIPT = "resource://gre/modules/addons/Content.js";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var gSingleton = null;

var gParentMM = null;


function amManager() {
  Cu.import("resource://gre/modules/AddonManager.jsm");
  /*globals AddonManagerPrivate*/

  let globalMM = Services.mm;
  globalMM.loadFrameScript(CHILD_SCRIPT, true);
  globalMM.addMessageListener(MSG_INSTALL_ADDONS, this);

  gParentMM = Services.ppmm;
  gParentMM.addMessageListener(MSG_INSTALL_ENABLED, this);
  gParentMM.addMessageListener(MSG_PROMISE_REQUEST, this);

  // Needed so receiveMessage can be called directly by JS callers
  this.wrappedJSObject = this;
}

amManager.prototype = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "addons-startup")
      AddonManagerPrivate.startup();
  },

  /**
   * @see amIAddonManager.idl
   */
  mapURIToAddonID: function(uri, id) {
    id.value = AddonManager.mapURIToAddonID(uri);
    return !!id.value;
  },

  /**
   * @see amIWebInstaller.idl
   */
  isInstallEnabled: function(aMimetype, aReferer) {
    return AddonManager.isInstallEnabled(aMimetype);
  },

  /**
   * @see amIWebInstaller.idl
   */
  installAddonsFromWebpage: function(aMimetype, aBrowser, aInstallingPrincipal,
                                     aUris, aHashes, aNames, aIcons, aCallback) {
    if (aUris.length == 0)
      return false;

    let retval = true;
    if (!AddonManager.isInstallAllowed(aMimetype, aInstallingPrincipal)) {
      aCallback = null;
      retval = false;
    }

    let installs = [];
    function buildNextInstall() {
      if (aUris.length == 0) {
        AddonManager.installAddonsFromWebpage(aMimetype, aBrowser, aInstallingPrincipal, installs);
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
      }, aMimetype, aHashes.shift(), aNames.shift(), aIcons.shift(), null, aBrowser);
    }
    buildNextInstall();

    return retval;
  },

  notify: function(aTimer) {
    AddonManagerPrivate.backgroundUpdateTimerHandler();
  },

  /**
   * messageManager callback function.
   *
   * Listens to requests from child processes for InstallTrigger
   * activity, and sends back callbacks.
   */
  receiveMessage: function(aMessage) {
    let payload = aMessage.data;

    switch (aMessage.name) {
      case MSG_INSTALL_ENABLED:
        return AddonManager.isInstallEnabled(payload.mimetype);

      case MSG_INSTALL_ADDONS: {
        let callback = null;
        if (payload.callbackID != -1) {
          callback = {
            onInstallEnded: function(url, status) {
              gParentMM.broadcastAsyncMessage(MSG_INSTALL_CALLBACK, {
                callbackID: payload.callbackID,
                url: url,
                status: status
              });
            },
          };
        }

        return this.installAddonsFromWebpage(payload.mimetype,
          aMessage.target, payload.triggeringPrincipal, payload.uris,
          payload.hashes, payload.names, payload.icons, callback);
      }

      case MSG_PROMISE_REQUEST: {
        let resolve = (value) => {
          aMessage.target.sendAsyncMessage(MSG_PROMISE_RESULT, {
            callbackID: payload.callbackID,
            resolve: value
          });
        }
        let reject = (value) => {
          aMessage.target.sendAsyncMessage(MSG_PROMISE_RESULT, {
            callbackID: payload.callbackID,
            reject: value
          });
        }

        let API = AddonManager.webAPI;
        if (payload.type in API) {
          API[payload.type](...payload.args).then(resolve, reject);
        }
        else {
          reject("Unknown Add-on API request.");
        }
        break;
      }
    }
    return undefined;
  },

  classID: Components.ID("{4399533d-08d1-458c-a87a-235f74451cfa}"),
  _xpcom_factory: {
    createInstance: function(aOuter, aIid) {
      if (aOuter != null)
        throw Components.Exception("Component does not support aggregation",
                                   Cr.NS_ERROR_NO_AGGREGATION);

      if (!gSingleton)
        gSingleton = new amManager();
      return gSingleton.QueryInterface(aIid);
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.amIAddonManager,
                                         Ci.amIWebInstaller,
                                         Ci.nsITimerCallback,
                                         Ci.nsIObserver,
                                         Ci.nsIMessageListener])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([amManager]);
