/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewProgress"];

const {GeckoViewModule} = ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this, "OverrideService", "@mozilla.org/security/certoverride;1",
  "nsICertOverrideService");

XPCOMUtils.defineLazyServiceGetter(
  this, "IDNService", "@mozilla.org/network/idn-service;1", "nsIIDNService");

var IdentityHandler = {
  // The definitions below should be kept in sync with those in GeckoView.ProgressListener.SecurityInformation
  // No trusted identity information. No site identity icon is shown.
  IDENTITY_MODE_UNKNOWN: 0,

  // Domain-Validation SSL CA-signed domain verification (DV).
  IDENTITY_MODE_IDENTIFIED: 1,

  // Extended-Validation SSL CA-signed identity information (EV). A more rigorous validation process.
  IDENTITY_MODE_VERIFIED: 2,

  // The following mixed content modes are only used if "security.mixed_content.block_active_content"
  // is enabled. Our Java frontend coalesces them into one indicator.

  // No mixed content information. No mixed content icon is shown.
  MIXED_MODE_UNKNOWN: 0,

  // Blocked active mixed content.
  MIXED_MODE_CONTENT_BLOCKED: 1,

  // Loaded active mixed content.
  MIXED_MODE_CONTENT_LOADED: 2,

  /**
   * Determines the identity mode corresponding to the icon we show in the urlbar.
   */
  getIdentityMode: function getIdentityMode(aState) {
    if (aState & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL) {
      return this.IDENTITY_MODE_VERIFIED;
    }

    if (aState & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
      return this.IDENTITY_MODE_IDENTIFIED;
    }

    return this.IDENTITY_MODE_UNKNOWN;
  },

  getMixedDisplayMode: function getMixedDisplayMode(aState) {
    if (aState & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT) {
      return this.MIXED_MODE_CONTENT_LOADED;
    }

    if (aState & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_DISPLAY_CONTENT) {
      return this.MIXED_MODE_CONTENT_BLOCKED;
    }

    return this.MIXED_MODE_UNKNOWN;
  },

  getMixedActiveMode: function getActiveDisplayMode(aState) {
    // Only show an indicator for loaded mixed content if the pref to block it is enabled
    if ((aState & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT) &&
        !Services.prefs.getBoolPref("security.mixed_content.block_active_content")) {
      return this.MIXED_MODE_CONTENT_LOADED;
    }

    if (aState & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT) {
      return this.MIXED_MODE_CONTENT_BLOCKED;
    }

    return this.MIXED_MODE_UNKNOWN;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available). Return the data needed to update the UI.
   */
  checkIdentity: function checkIdentity(aState, aBrowser) {
    const identityMode = this.getIdentityMode(aState);
    const mixedDisplay = this.getMixedDisplayMode(aState);
    const mixedActive = this.getMixedActiveMode(aState);
    const result = {
      mode: {
        identity: identityMode,
        mixed_display: mixedDisplay,
        mixed_active: mixedActive,
      },
    };

    if (aBrowser.contentPrincipal) {
      result.origin = aBrowser.contentPrincipal.originNoSuffix;
    }

    // Don't show identity data for pages with an unknown identity or if any
    // mixed content is loaded (mixed display content is loaded by default).
    if (identityMode === this.IDENTITY_MODE_UNKNOWN ||
        (aState & Ci.nsIWebProgressListener.STATE_IS_BROKEN) ||
        (aState & Ci.nsIWebProgressListener.STATE_IS_INSECURE)) {
      result.secure = false;
      return result;
    }

    result.secure = true;

    let uri = aBrowser.currentURI || {};
    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}

    try {
      result.host = IDNService.convertToDisplayIDN(uri.host, {});
    } catch (e) {
      result.host = uri.host;
    }

    const cert = aBrowser.securityUI.secInfo.serverCert;

    result.organization = cert.organization;
    result.subjectName = cert.subjectName;
    result.issuerOrganization = cert.issuerOrganization;
    result.issuerCommonName = cert.issuerCommonName;

    try {
      result.securityException = OverrideService.hasMatchingOverride(
        uri.host, uri.port, cert, {}, {});
    } catch (e) {
    }

    return result;
  },
};

class GeckoViewProgress extends GeckoViewModule {
  onInit() {
    this._hostChanged = false;
  }

  onEnable() {
    debug `onEnable`;

    const flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK |
                  Ci.nsIWebProgress.NOTIFY_SECURITY |
                  Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
    Services.obs.addObserver(this, "oop-frameloader-crashed");
    this.registerListener("GeckoView:FlushSessionState");
  }

  onDisable() {
    debug `onDisable`;

    if (this.progressFilter) {
      this.progressFilter.removeProgressListener(this);
      this.browser.removeProgressListener(this.progressFilter);
    }

    Services.obs.removeObserver(this, "oop-frameloader-crashed");
    this.unregisterListener("GeckoView:FlushSessionState");
  }

  onEvent(aEvent, aData, aCallback) {
    debug `onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:FlushSessionState":
        this.messageManager.sendAsyncMessage("GeckoView:FlushSessionState");
        break;
    }
  }

  onSettingsUpdate() {
    const settings = this.settings;
    debug `onSettingsUpdate: ${settings}`;
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    debug `onStateChange: isTopLevel=${aWebProgress.isTopLevel},
                          flags=${aStateFlags}, status=${aStatus}
                          loadType=${aWebProgress.loadType}`;


    if (!aWebProgress.isTopLevel) {
      return;
    }

    const uriSpec = aRequest.QueryInterface(Ci.nsIChannel).URI.displaySpec;
    const isSuccess = aStatus == Cr.NS_OK;
    const isStart = (aStateFlags & Ci.nsIWebProgressListener.STATE_START) != 0;
    const isStop = (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) != 0;

    debug `onStateChange: uri=${uriSpec} isSuccess=${isSuccess}
           isStart=${isStart} isStop=${isStop}`;

    if (isStart) {
      this._inProgress = true;
      const message = {
        type: "GeckoView:PageStart",
        uri: uriSpec,
      };

      this.eventDispatcher.sendRequest(message);
    } else if (isStop && !aWebProgress.isLoadingDocument) {
      this._inProgress = false;

      const message = {
        type: "GeckoView:PageStop",
        success: isSuccess,
      };

      this.eventDispatcher.sendRequest(message);
    }
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    debug `onSecurityChange`;

    // Don't need to do anything if the data we use to update the UI hasn't changed
    if (this._state === aState && !this._hostChanged) {
      return;
    }

    this._state = aState;
    this._hostChanged = false;

    const identity = IdentityHandler.checkIdentity(aState, this.browser);

    const message = {
      type: "GeckoView:SecurityChanged",
      identity: identity,
    };

    this.eventDispatcher.sendRequest(message);
  }

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug `onLocationChange: location=${aLocationURI.displaySpec},
                             flags=${aFlags}`;

    this._hostChanged = true;
  }

  // nsIObserver event handler
  observe(aSubject, aTopic, aData) {
    debug `observe: topic=${aTopic}`;

    switch (aTopic) {
      case "oop-frameloader-crashed": {
        const browser = aSubject.ownerElement;
        if (!browser || browser != this.browser || !this._inProgress) {
          return;
        }

        this.eventDispatcher.sendRequest({
          type: "GeckoView:PageStop",
          success: false,
        });
      }
    }
  }
}

const {debug, warn} = GeckoViewProgress.initLogging("GeckoViewProgress"); // eslint-disable-line no-unused-vars
