/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component serves as integration between the platform and AddonManager.
 * It is responsible for initializing and shutting down the AddonManager as well
 * as passing new installs from webpages to the AddonManager.
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "separatePrivilegedMozillaWebContentProcess",
  "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "extensionsWebAPITesting",
  "extensions.webapi.testing",
  false
);

// The old XPInstall error codes
const EXECUTION_ERROR = -203;
const CANT_READ_ARCHIVE = -207;
const USER_CANCELLED = -210;
const DOWNLOAD_ERROR = -228;
const UNSUPPORTED_TYPE = -244;
const SUCCESS = 0;

const MSG_INSTALL_ENABLED = "WebInstallerIsInstallEnabled";
const MSG_INSTALL_ADDON = "WebInstallerInstallAddonFromWebpage";
const MSG_INSTALL_CALLBACK = "WebInstallerInstallCallback";

const MSG_PROMISE_REQUEST = "WebAPIPromiseRequest";
const MSG_PROMISE_RESULT = "WebAPIPromiseResult";
const MSG_INSTALL_EVENT = "WebAPIInstallEvent";
const MSG_INSTALL_CLEANUP = "WebAPICleanup";
const MSG_ADDON_EVENT_REQ = "WebAPIAddonEventRequest";
const MSG_ADDON_EVENT = "WebAPIAddonEvent";

const CHILD_SCRIPT = "resource://gre/modules/addons/Content.js";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var gSingleton = null;

var AddonManager, AddonManagerPrivate;
function amManager() {
  ({ AddonManager, AddonManagerPrivate } = ChromeUtils.import(
    "resource://gre/modules/AddonManager.jsm"
  ));

  Services.mm.loadFrameScript(CHILD_SCRIPT, true, true);
  Services.mm.addMessageListener(MSG_INSTALL_ENABLED, this);
  Services.mm.addMessageListener(MSG_PROMISE_REQUEST, this);
  Services.mm.addMessageListener(MSG_INSTALL_CLEANUP, this);
  Services.mm.addMessageListener(MSG_ADDON_EVENT_REQ, this);

  Services.ppmm.addMessageListener(MSG_INSTALL_ADDON, this);

  Services.obs.addObserver(this, "message-manager-close");
  Services.obs.addObserver(this, "message-manager-disconnect");

  AddonManager.webAPI.setEventHandler(this.sendEvent);

  // Needed so receiveMessage can be called directly by JS callers
  this.wrappedJSObject = this;
}

amManager.prototype = {
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "addons-startup":
        AddonManagerPrivate.startup();
        break;

      case "message-manager-close":
      case "message-manager-disconnect":
        this.childClosed(aSubject);
        break;
    }
  },

  installAddonFromWebpage(aPayload, aBrowser, aCallback) {
    let retval = true;

    const { mimetype, triggeringPrincipal, hash, icon, name, uri } = aPayload;

    if (!AddonManager.isInstallAllowed(mimetype, triggeringPrincipal)) {
      aCallback = null;
      retval = false;
    }

    let telemetryInfo = {
      source: AddonManager.getInstallSourceFromHost(aPayload.sourceHost),
      sourceURL: aPayload.sourceURL,
    };

    if ("method" in aPayload) {
      telemetryInfo.method = aPayload.method;
    }

    AddonManager.getInstallForURL(uri, {
      hash,
      name,
      icon,
      browser: aBrowser,
      triggeringPrincipal,
      telemetryInfo,
      sendCookies: true,
    }).then(aInstall => {
      function callCallback(status) {
        try {
          aCallback.onInstallEnded(uri, status);
        } catch (e) {
          Cu.reportError(e);
        }
      }

      if (!aInstall) {
        aCallback.onInstallEnded(uri, UNSUPPORTED_TYPE);
        return;
      }

      if (aCallback) {
        aInstall.addListener({
          onDownloadCancelled(aInstall) {
            callCallback(USER_CANCELLED);
          },

          onDownloadFailed(aInstall) {
            if (aInstall.error == AddonManager.ERROR_CORRUPT_FILE) {
              callCallback(CANT_READ_ARCHIVE);
            } else {
              callCallback(DOWNLOAD_ERROR);
            }
          },

          onInstallFailed(aInstall) {
            callCallback(EXECUTION_ERROR);
          },

          onInstallEnded(aInstall, aStatus) {
            callCallback(SUCCESS);
          },
        });
      }

      AddonManager.installAddonFromWebpage(
        mimetype,
        aBrowser,
        triggeringPrincipal,
        aInstall
      );
    });

    return retval;
  },

  notify(aTimer) {
    AddonManagerPrivate.backgroundUpdateTimerHandler();
  },

  // Maps message manager instances for content processes to the associated
  // AddonListener instances.
  addonListeners: new Map(),

  _addAddonListener(target) {
    if (!this.addonListeners.has(target)) {
      let handler = (event, id) => {
        target.sendAsyncMessage(MSG_ADDON_EVENT, { event, id });
      };
      let listener = {
        onEnabling: addon => handler("onEnabling", addon.id),
        onEnabled: addon => handler("onEnabled", addon.id),
        onDisabling: addon => handler("onDisabling", addon.id),
        onDisabled: addon => handler("onDisabled", addon.id),
        onInstalling: addon => handler("onInstalling", addon.id),
        onInstalled: addon => handler("onInstalled", addon.id),
        onUninstalling: addon => handler("onUninstalling", addon.id),
        onUninstalled: addon => handler("onUninstalled", addon.id),
        onOperationCancelled: addon =>
          handler("onOperationCancelled", addon.id),
      };
      this.addonListeners.set(target, listener);
      AddonManager.addAddonListener(listener);
    }
  },

  _removeAddonListener(target) {
    if (this.addonListeners.has(target)) {
      AddonManager.removeAddonListener(this.addonListeners.get(target));
      this.addonListeners.delete(target);
    }
  },

  /**
   * messageManager callback function.
   *
   * Listens to requests from child processes for InstallTrigger
   * activity, and sends back callbacks.
   */
  receiveMessage(aMessage) {
    let payload = aMessage.data;

    switch (aMessage.name) {
      case MSG_INSTALL_ENABLED:
        return AddonManager.isInstallEnabled(payload.mimetype);

      case MSG_INSTALL_ADDON: {
        let browser = payload.browsingContext.top.embedderElement;

        let callback = null;
        if (payload.callbackID != -1) {
          let mm = browser.messageManager;
          callback = {
            onInstallEnded(url, status) {
              mm.sendAsyncMessage(MSG_INSTALL_CALLBACK, {
                callbackID: payload.callbackID,
                url,
                status,
              });
            },
          };
        }

        return this.installAddonFromWebpage(payload, browser, callback);
      }

      case MSG_PROMISE_REQUEST: {
        if (
          !extensionsWebAPITesting &&
          separatePrivilegedMozillaWebContentProcess &&
          aMessage.target &&
          aMessage.target.remoteType != null &&
          aMessage.target.remoteType !== "privilegedmozilla"
        ) {
          return undefined;
        }

        let mm = aMessage.target.messageManager;
        let resolve = value => {
          mm.sendAsyncMessage(MSG_PROMISE_RESULT, {
            callbackID: payload.callbackID,
            resolve: value,
          });
        };
        let reject = value => {
          mm.sendAsyncMessage(MSG_PROMISE_RESULT, {
            callbackID: payload.callbackID,
            reject: value,
          });
        };

        let API = AddonManager.webAPI;
        if (payload.type in API) {
          API[payload.type](aMessage.target, ...payload.args).then(
            resolve,
            reject
          );
        } else {
          reject("Unknown Add-on API request.");
        }
        break;
      }

      case MSG_INSTALL_CLEANUP: {
        if (
          !extensionsWebAPITesting &&
          separatePrivilegedMozillaWebContentProcess &&
          aMessage.target &&
          aMessage.target.remoteType != null &&
          aMessage.target.remoteType !== "privilegedmozilla"
        ) {
          return undefined;
        }

        AddonManager.webAPI.clearInstalls(payload.ids);
        break;
      }

      case MSG_ADDON_EVENT_REQ: {
        if (
          !extensionsWebAPITesting &&
          separatePrivilegedMozillaWebContentProcess &&
          aMessage.target &&
          aMessage.target.remoteType != null &&
          aMessage.target.remoteType !== "privilegedmozilla"
        ) {
          return undefined;
        }

        let target = aMessage.target.messageManager;
        if (payload.enabled) {
          this._addAddonListener(target);
        } else {
          this._removeAddonListener(target);
        }
      }
    }
    return undefined;
  },

  childClosed(target) {
    AddonManager.webAPI.clearInstallsFrom(target);
    this._removeAddonListener(target);
  },

  sendEvent(mm, data) {
    mm.sendAsyncMessage(MSG_INSTALL_EVENT, data);
  },

  classID: Components.ID("{4399533d-08d1-458c-a87a-235f74451cfa}"),
  _xpcom_factory: {
    createInstance(aOuter, aIid) {
      if (aOuter != null) {
        throw Components.Exception(
          "Component does not support aggregation",
          Cr.NS_ERROR_NO_AGGREGATION
        );
      }

      if (!gSingleton) {
        gSingleton = new amManager();
      }
      return gSingleton.QueryInterface(aIid);
    },
  },
  QueryInterface: ChromeUtils.generateQI([
    "amIAddonManager",
    "nsITimerCallback",
    "nsIObserver",
  ]),
};

const BLOCKLIST_JSM = "resource://gre/modules/Blocklist.jsm";
ChromeUtils.defineModuleGetter(this, "Blocklist", BLOCKLIST_JSM);

function BlocklistService() {
  this.wrappedJSObject = this;
  this.pluginQueries = [];
}

BlocklistService.prototype = {
  STATE_NOT_BLOCKED: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
  STATE_SOFTBLOCKED: Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
  STATE_BLOCKED: Ci.nsIBlocklistService.STATE_BLOCKED,
  STATE_OUTDATED: Ci.nsIBlocklistService.STATE_OUTDATED,
  STATE_VULNERABLE_UPDATE_AVAILABLE:
    Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE,
  STATE_VULNERABLE_NO_UPDATE: Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE,

  get isLoaded() {
    return Cu.isModuleLoaded(BLOCKLIST_JSM) && Blocklist.isLoaded;
  },

  async getPluginBlocklistState(plugin, appVersion, toolkitVersion) {
    if (AppConstants.platform == "android") {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }
    if (Cu.isModuleLoaded(BLOCKLIST_JSM)) {
      return Blocklist.getPluginBlocklistState(
        plugin,
        appVersion,
        toolkitVersion
      );
    }

    // Blocklist module isn't loaded yet. Queue the query until it is.
    let request = { plugin, appVersion, toolkitVersion };
    let promise = new Promise(resolve => {
      request.resolve = resolve;
    });

    this.pluginQueries.push(request);
    return promise;
  },

  observe(...args) {
    return Blocklist.observe(...args);
  },

  notify() {
    Blocklist.notify();
  },

  classID: Components.ID("{66354bc9-7ed1-4692-ae1d-8da97d6b205e}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsIBlocklistService",
    "nsITimerCallback",
  ]),
};

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["amManager", "BlocklistService"];
