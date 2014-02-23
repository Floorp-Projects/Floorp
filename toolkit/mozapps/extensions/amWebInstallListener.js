/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a default implementation of amIWebInstallListener that should work
 * for most applications but can be overriden. It notifies the observer service
 * about blocked installs. For normal installs it pops up an install
 * confirmation when all the add-ons have been downloaded.
 */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const URI_XPINSTALL_DIALOG = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";

// Installation can begin from any of these states
const READY_STATES = [
  AddonManager.STATE_AVAILABLE,
  AddonManager.STATE_DOWNLOAD_FAILED,
  AddonManager.STATE_INSTALL_FAILED,
  AddonManager.STATE_CANCELLED
];

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.weblistener";

// Create a new logger for use by the Addons Web Listener
// (Requires AddonManager.jsm)
let logger = Log.repository.getLogger(LOGGER_ID);

function notifyObservers(aTopic, aWindow, aUri, aInstalls) {
  let info = {
    originatingWindow: aWindow,
    originatingURI: aUri,
    installs: aInstalls,

    QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
  };
  Services.obs.notifyObservers(info, aTopic, null);
}

/**
 * Creates a new installer to monitor downloads and prompt to install when
 * ready
 *
 * @param  aWindow
 *         The window that started the installations
 * @param  aUrl
 *         The URL that started the installations
 * @param  aInstalls
 *         An array of AddonInstalls
 */
function Installer(aWindow, aUrl, aInstalls) {
  this.window = aWindow;
  this.url = aUrl;
  this.downloads = aInstalls;
  this.installed = [];

  notifyObservers("addon-install-started", aWindow, aUrl, aInstalls);

  aInstalls.forEach(function(aInstall) {
    aInstall.addListener(this);

    // Start downloading if it hasn't already begun
    if (READY_STATES.indexOf(aInstall.state) != -1)
      aInstall.install();
  }, this);

  this.checkAllDownloaded();
}

Installer.prototype = {
  window: null,
  downloads: null,
  installed: null,
  isDownloading: true,

  /**
   * Checks if all downloads are now complete and if so prompts to install.
   */
  checkAllDownloaded: function Installer_checkAllDownloaded() {
    // Prevent re-entrancy caused by the confirmation dialog cancelling unwanted
    // installs.
    if (!this.isDownloading)
      return;

    var failed = [];
    var installs = [];

    for (let install of this.downloads) {
      switch (install.state) {
      case AddonManager.STATE_AVAILABLE:
      case AddonManager.STATE_DOWNLOADING:
        // Exit early if any add-ons haven't started downloading yet or are
        // still downloading
        return;
      case AddonManager.STATE_DOWNLOAD_FAILED:
        failed.push(install);
        break;
      case AddonManager.STATE_DOWNLOADED:
        // App disabled items are not compatible and so fail to install
        if (install.addon.appDisabled)
          failed.push(install);
        else
          installs.push(install);

        if (install.linkedInstalls) {
          install.linkedInstalls.forEach(function(aInstall) {
            aInstall.addListener(this);
            // App disabled items are not compatible and so fail to install
            if (aInstall.addon.appDisabled)
              failed.push(aInstall);
            else
              installs.push(aInstall);
          }, this);
        }
        break;
      case AddonManager.STATE_CANCELLED:
        // Just ignore cancelled downloads
        break;
      default:
        logger.warn("Download of " + install.sourceURI.spec + " in unexpected state " +
             install.state);
      }
    }

    this.isDownloading = false;
    this.downloads = installs;

    if (failed.length > 0) {
      // Stop listening and cancel any installs that are failed because of
      // compatibility reasons.
      failed.forEach(function(aInstall) {
        if (aInstall.state == AddonManager.STATE_DOWNLOADED) {
          aInstall.removeListener(this);
          aInstall.cancel();
        }
      }, this);
      notifyObservers("addon-install-failed", this.window, this.url, failed);
    }

    // If none of the downloads were successful then exit early
    if (this.downloads.length == 0)
      return;

    // Check for a custom installation prompt that may be provided by the
    // applicaton
    if ("@mozilla.org/addons/web-install-prompt;1" in Cc) {
      try {
        let prompt = Cc["@mozilla.org/addons/web-install-prompt;1"].
                     getService(Ci.amIWebInstallPrompt);
        prompt.confirm(this.window, this.url, this.downloads, this.downloads.length);
        return;
      }
      catch (e) {}
    }

    let args = {};
    args.url = this.url;
    args.installs = this.downloads;
    args.wrappedJSObject = args;

    try {
      Cc["@mozilla.org/base/telemetry;1"].
            getService(Ci.nsITelemetry).
            getHistogramById("SECURITY_UI").
            add(Ci.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL);
      Services.ww.openWindow(this.window, URI_XPINSTALL_DIALOG,
                             null, "chrome,modal,centerscreen", args);
    } catch (e) {
      this.downloads.forEach(function(aInstall) {
        aInstall.removeListener(this);
        // Cancel the installs, as currently there is no way to make them fail
        // from here.
        aInstall.cancel();
      }, this);
      notifyObservers("addon-install-cancelled", this.window, this.url,
                      this.downloads);
    }
  },

  /**
   * Checks if all installs are now complete and if so notifies observers.
   */
  checkAllInstalled: function Installer_checkAllInstalled() {
    var failed = [];

    for (let install of this.downloads) {
      switch(install.state) {
      case AddonManager.STATE_DOWNLOADED:
      case AddonManager.STATE_INSTALLING:
        // Exit early if any add-ons haven't started installing yet or are
        // still installing
        return;
      case AddonManager.STATE_INSTALL_FAILED:
        failed.push(install);
        break;
      }
    }

    this.downloads = null;

    if (failed.length > 0)
      notifyObservers("addon-install-failed", this.window, this.url, failed);

    if (this.installed.length > 0)
      notifyObservers("addon-install-complete", this.window, this.url, this.installed);
    this.installed = null;
  },

  onDownloadCancelled: function Installer_onDownloadCancelled(aInstall) {
    aInstall.removeListener(this);
    this.checkAllDownloaded();
  },

  onDownloadFailed: function Installer_onDownloadFailed(aInstall) {
    aInstall.removeListener(this);
    this.checkAllDownloaded();
  },

  onDownloadEnded: function Installer_onDownloadEnded(aInstall) {
    this.checkAllDownloaded();
    return false;
  },

  onInstallCancelled: function Installer_onInstallCancelled(aInstall) {
    aInstall.removeListener(this);
    this.checkAllInstalled();
  },

  onInstallFailed: function Installer_onInstallFailed(aInstall) {
    aInstall.removeListener(this);
    this.checkAllInstalled();
  },

  onInstallEnded: function Installer_onInstallEnded(aInstall) {
    aInstall.removeListener(this);
    this.installed.push(aInstall);

    // If installing a theme that is disabled and can be enabled then enable it
    if (aInstall.addon.type == "theme" &&
        aInstall.addon.userDisabled == true &&
        aInstall.addon.appDisabled == false) {
      aInstall.addon.userDisabled = false;
    }

    this.checkAllInstalled();
  }
};

function extWebInstallListener() {
}

extWebInstallListener.prototype = {
  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallDisabled: function extWebInstallListener_onWebInstallDisabled(aWindow, aUri, aInstalls) {
    let info = {
      originatingWindow: aWindow,
      originatingURI: aUri,
      installs: aInstalls,

      QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
    };
    Services.obs.notifyObservers(info, "addon-install-disabled", null);
  },

  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallBlocked: function extWebInstallListener_onWebInstallBlocked(aWindow, aUri, aInstalls) {
    let info = {
      originatingWindow: aWindow,
      originatingURI: aUri,
      installs: aInstalls,

      install: function onWebInstallBlocked_install() {
        new Installer(this.originatingWindow, this.originatingURI, this.installs);
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
    };
    Services.obs.notifyObservers(info, "addon-install-blocked", null);

    return false;
  },

  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallRequested: function extWebInstallListener_onWebInstallRequested(aWindow, aUri, aInstalls) {
    new Installer(aWindow, aUri, aInstalls);

    // We start the installs ourself
    return false;
  },

  classDescription: "XPI Install Handler",
  contractID: "@mozilla.org/addons/web-install-listener;1",
  classID: Components.ID("{0f38e086-89a3-40a5-8ffc-9b694de1d04a}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallListener])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([extWebInstallListener]);
