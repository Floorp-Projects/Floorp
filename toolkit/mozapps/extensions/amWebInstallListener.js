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
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PromptUtils", "resource://gre/modules/SharedPromptUtils.jsm");

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
var logger = Log.repository.getLogger(LOGGER_ID);

function notifyObservers(aTopic, aBrowser, aUri, aInstalls) {
  let info = {
    browser: aBrowser,
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
 * @param  aBrowser
 *         The browser that started the installations
 * @param  aUrl
 *         The URL that started the installations
 * @param  aInstalls
 *         An array of AddonInstalls
 */
function Installer(aBrowser, aUrl, aInstalls) {
  this.browser = aBrowser;
  this.url = aUrl;
  this.downloads = aInstalls;
  this.installed = [];

  notifyObservers("addon-install-started", aBrowser, aUrl, aInstalls);

  for (let install of aInstalls) {
    install.addListener(this);

    // Start downloading if it hasn't already begun
    if (READY_STATES.indexOf(install.state) != -1)
      install.install();
  }

  this.checkAllDownloaded();
}

Installer.prototype = {
  browser: null,
  downloads: null,
  installed: null,
  isDownloading: true,

  /**
   * Checks if all downloads are now complete and if so prompts to install.
   */
  checkAllDownloaded: function() {
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
      for (let install of failed) {
        if (install.state == AddonManager.STATE_DOWNLOADED) {
          install.removeListener(this);
          install.cancel();
        }
      }
      notifyObservers("addon-install-failed", this.browser, this.url, failed);
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
        prompt.confirm(this.browser, this.url, this.downloads, this.downloads.length);
        return;
      }
      catch (e) {}
    }

    if (Preferences.get("xpinstall.customConfirmationUI", false)) {
      notifyObservers("addon-install-confirmation", this.browser, this.url, this.downloads);
      return;
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
      let parentWindow = null;
      if (this.browser) {
        parentWindow = this.browser.ownerDocument.defaultView;
        PromptUtils.fireDialogEvent(parentWindow, "DOMWillOpenModalDialog", this.browser);
      }
      Services.ww.openWindow(parentWindow, URI_XPINSTALL_DIALOG,
                             null, "chrome,modal,centerscreen", args);
    } catch (e) {
      logger.warn("Exception showing install confirmation dialog", e);
      for (let install of this.downloads) {
        install.removeListener(this);
        // Cancel the installs, as currently there is no way to make them fail
        // from here.
        install.cancel();
      }
      notifyObservers("addon-install-cancelled", this.browser, this.url,
                      this.downloads);
    }
  },

  /**
   * Checks if all installs are now complete and if so notifies observers.
   */
  checkAllInstalled: function() {
    var failed = [];

    for (let install of this.downloads) {
      switch (install.state) {
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
      notifyObservers("addon-install-failed", this.browser, this.url, failed);

    if (this.installed.length > 0)
      notifyObservers("addon-install-complete", this.browser, this.url, this.installed);
    this.installed = null;
  },

  onDownloadCancelled: function(aInstall) {
    aInstall.removeListener(this);
    this.checkAllDownloaded();
  },

  onDownloadFailed: function(aInstall) {
    aInstall.removeListener(this);
    this.checkAllDownloaded();
  },

  onDownloadEnded: function(aInstall) {
    this.checkAllDownloaded();
    return false;
  },

  onInstallCancelled: function(aInstall) {
    aInstall.removeListener(this);
    this.checkAllInstalled();
  },

  onInstallFailed: function(aInstall) {
    aInstall.removeListener(this);
    this.checkAllInstalled();
  },

  onInstallEnded: function(aInstall) {
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
  onWebInstallDisabled: function(aBrowser, aUri, aInstalls) {
    let info = {
      browser: aBrowser,
      originatingURI: aUri,
      installs: aInstalls,

      QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
    };
    Services.obs.notifyObservers(info, "addon-install-disabled", null);
  },

  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallOriginBlocked: function(aBrowser, aUri, aInstalls) {
    let info = {
      browser: aBrowser,
      originatingURI: aUri,
      installs: aInstalls,

      install: function() {
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
    };
    Services.obs.notifyObservers(info, "addon-install-origin-blocked", null);

    return false;
  },

  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallBlocked: function(aBrowser, aUri, aInstalls) {
    let info = {
      browser: aBrowser,
      originatingURI: aUri,
      installs: aInstalls,

      install: function() {
        new Installer(this.browser, this.originatingURI, this.installs);
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
    };
    Services.obs.notifyObservers(info, "addon-install-blocked", null);

    return false;
  },

  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallRequested: function(aBrowser, aUri, aInstalls) {
    new Installer(aBrowser, aUri, aInstalls);

    // We start the installs ourself
    return false;
  },

  classDescription: "XPI Install Handler",
  contractID: "@mozilla.org/addons/web-install-listener;1",
  classID: Components.ID("{0f38e086-89a3-40a5-8ffc-9b694de1d04a}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallListener,
                                         Ci.amIWebInstallListener2])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([extWebInstallListener]);
