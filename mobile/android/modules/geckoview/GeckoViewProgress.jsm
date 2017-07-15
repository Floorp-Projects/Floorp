/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewProgress"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewProgress");

function debug(aMsg) {
  // dump(aMsg);
}

var IdentityHandler = {
  // No trusted identity information. No site identity icon is shown.
  IDENTITY_MODE_UNKNOWN: "unknown",

  // Domain-Validation SSL CA-signed domain verification (DV).
  IDENTITY_MODE_IDENTIFIED: "identified",

  // Extended-Validation SSL CA-signed identity information (EV). A more rigorous validation process.
  IDENTITY_MODE_VERIFIED: "verified",

  // The following mixed content modes are only used if "security.mixed_content.block_active_content"
  // is enabled. Our Java frontend coalesces them into one indicator.

  // No mixed content information. No mixed content icon is shown.
  MIXED_MODE_UNKNOWN: "unknown",

  // Blocked active mixed content.
  MIXED_MODE_CONTENT_BLOCKED: "blocked",

  // Loaded active mixed content.
  MIXED_MODE_CONTENT_LOADED: "loaded",

  // The following tracking content modes are only used if tracking protection
  // is enabled. Our Java frontend coalesces them into one indicator.

  // No tracking content information. No tracking content icon is shown.
  TRACKING_MODE_UNKNOWN: "unknown",

  // Blocked active tracking content. Shield icon is shown, with a popup option to load content.
  TRACKING_MODE_CONTENT_BLOCKED: "blocked",

  // Loaded active tracking content. Yellow triangle icon is shown.
  TRACKING_MODE_CONTENT_LOADED: "loaded",

  _useTrackingProtection : false,
  _usePrivateMode : false,

  setUseTrackingProtection : function(aUse) {
    this._useTrackingProtection = aUse;
  },

  setUsePrivateMode : function(aUse) {
    this._usePrivateMode = aUse;
  },

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

  getTrackingMode: function getTrackingMode(aState) {
    if (aState & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) {
      return this.TRACKING_MODE_CONTENT_BLOCKED;
    }

    // Only show an indicator for loaded tracking content if the pref to block it is enabled
    if ((aState & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) && this._useTrackingProtection) {
      return this.TRACKING_MODE_CONTENT_LOADED;
    }

    return this.TRACKING_MODE_UNKNOWN;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available). Return the data needed to update the UI.
   */
  checkIdentity: function checkIdentity(aState, aBrowser) {
    let lastStatus = aBrowser.securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    // Don't pass in the actual location object, since it can cause us to
    // hold on to the window object too long.  Just pass in the fields we
    // care about. (bug 424829)
    let lastLocation = {};
    try {
      let location = aBrowser.contentWindow.location;
      lastLocation.host = location.host;
      lastLocation.hostname = location.hostname;
      lastLocation.port = location.port;
      lastLocation.origin = location.origin;
    } catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }

    let uri = aBrowser.currentURI;
    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}

    let identityMode = this.getIdentityMode(aState);
    let mixedDisplay = this.getMixedDisplayMode(aState);
    let mixedActive = this.getMixedActiveMode(aState);
    let trackingMode = this.getTrackingMode(aState);
    let result = {
      origin: lastLocation.origin,
      mode: {
        identity: identityMode,
        mixed_display: mixedDisplay,
        mixed_active: mixedActive,
        tracking: trackingMode
      }
    };

    // Don't show identity data for pages with an unknown identity or if any
    // mixed content is loaded (mixed display content is loaded by default).
    if (identityMode === this.IDENTITY_MODE_UNKNOWN ||
        aState & Ci.nsIWebProgressListener.STATE_IS_BROKEN) {
      result.secure = false;
      return result;
    }

    result.secure = true;

    result.host = this.getEffectiveHost(lastLocation, uri);

    let status = lastStatus.QueryInterface(Ci.nsISSLStatus);
    let cert = status.serverCert;

    result.organization = cert.organization;
    result.subjectName = cert.subjectName;
    result.issuerOrganization = cert.issuerOrganization;
    result.issuerCommonName = cert.issuerCommonName;

    // Cache the override service the first time we need to check it
    if (!this._overrideService) {
      this._overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(Ci.nsICertOverrideService);
    }

    // Check whether this site is a security exception. XPConnect does the right
    // thing here in terms of converting lastLocation.port from string to int, but
    // the overrideService doesn't like undefined ports, so make sure we have
    // something in the default case (bug 432241).
    // .hostname can return an empty string in some exceptional cases -
    // hasMatchingOverride does not handle that, so avoid calling it.
    // Updating the tooltip value in those cases isn't critical.
    // FIXME: Fixing bug 646690 would probably makes this check unnecessary
    if (lastLocation.hostname &&
        this._overrideService.hasMatchingOverride(lastLocation.hostname,
                                                  (lastLocation.port || 443),
                                                  cert, {}, {})) {
      result.security_exception = true;
    }
    return result;
  },

  /**
   * Attempt to provide proper IDN treatment for host names
   */
  getEffectiveHost: function getEffectiveHost(aLastLocation, aUri) {
    if (!this._IDNService) {
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
    }
    try {
      return this._IDNService.convertToDisplayIDN(aUri.host, {});
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return aLastLocation.hostname;
    }
  }
};

class GeckoViewProgress extends GeckoViewModule {
  init() {
    this._hostChanged = false;
  }

  register() {
    debug("register");

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK |
                Ci.nsIWebProgress.NOTIFY_SECURITY |
                Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  unregister() {
    debug("unregister");

    if (this.progressFilter) {
      this.progressFilter.removeProgressListener(this);
      this.browser.removeProgressListener(this.progressFilter);
    }
  }

  onSettingsUpdate() {
    let settings = this.settings;
    debug("onSettingsUpdate: " + JSON.stringify(settings));

    IdentityHandler.setUseTrackingProtection(!!settings.useTrackingProtection);
    IdentityHandler.setUsePrivateMode(!!settings.usePrivateMode);
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    debug("onStateChange()");

    if (!aWebProgress.isTopLevel) {
      return;
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      let uri = aRequest.QueryInterface(Ci.nsIChannel).URI;
      let message = {
        type: "GeckoView:PageStart",
        uri: uri.spec,
      };

      this.eventDispatcher.sendRequest(message);
    } else if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
               !aWebProgress.isLoadingDocument) {
      let message = {
        type: "GeckoView:PageStop",
        success: !aStatus
      };

      this.eventDispatcher.sendRequest(message);
    }
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't changed
    if (this._state === aState && !this._hostChanged) {
      return;
    }

    this._state = aState;
    this._hostChanged = false;

    let identity = IdentityHandler.checkIdentity(aState, this.browser);

    let message = {
      type: "GeckoView:SecurityChanged",
      status: aState,
      identity: identity
    };

    this.eventDispatcher.sendRequest(message);
  }

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    this._hostChanged = true;
  }
}
