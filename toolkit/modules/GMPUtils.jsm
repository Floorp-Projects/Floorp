/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu, manager: Cm} =
  Components;

this.EXPORTED_SYMBOLS = [ "EME_ADOBE_ID",
                          "GMP_PLUGIN_IDS",
                          "GMPPrefs",
                          "GMPUtils",
                          "OPEN_H264_ID",
                          "WIDEVINE_ID" ];

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

// GMP IDs
const OPEN_H264_ID  = "gmp-gmpopenh264";
const EME_ADOBE_ID  = "gmp-eme-adobe";
const WIDEVINE_ID   = "gmp-widevinecdm";
const GMP_PLUGIN_IDS = [ OPEN_H264_ID, EME_ADOBE_ID, WIDEVINE_ID ];

var GMPPluginUnsupportedReason = {
  NOT_WINDOWS: 1,
  WINDOWS_VERSION: 2,
};

var GMPPluginHiddenReason = {
  UNSUPPORTED: 1,
  EME_DISABLED: 2,
};

this.GMPUtils = {
  /**
   * Checks whether or not a given plugin is hidden. Hidden plugins are neither
   * downloaded nor displayed in the addons manager.
   * @param   aPlugin
   *          The plugin to check.
   */
  isPluginHidden(aPlugin) {
    if (this._is32bitModeMacOS()) {
      // GMPs are hidden on MacOS when running in 32 bit mode.
      // See bug 1291537.
      return true;
    }
    if (!aPlugin.isEME) {
      return false;
    }

    if (!this._isPluginSupported(aPlugin) ||
        !this._isPluginVisible(aPlugin)) {
      return true;
    }

    if (!GMPPrefs.get(GMPPrefs.KEY_EME_ENABLED, true)) {
      return true;
    }

    return false;
  },

  /**
   * Checks whether or not a given plugin is supported by the current OS.
   * @param   aPlugin
   *          The plugin to check.
   */
  _isPluginSupported(aPlugin) {
    if (this._isPluginForceSupported(aPlugin)) {
      return true;
    }
    if (aPlugin.id == EME_ADOBE_ID) {
      // Windows Vista and later only supported by Adobe EME.
      return AppConstants.isPlatformAndVersionAtLeast("win", "6");
    } else if (aPlugin.id == WIDEVINE_ID) {
      // The Widevine plugin is available for Windows versions Vista and later,
      // Mac OSX, and Linux.
      return AppConstants.isPlatformAndVersionAtLeast("win", "6") ||
             AppConstants.platform == "macosx" ||
             AppConstants.platform == "linux";
    }

    return true;
  },

  _is32bitModeMacOS() {
    if (AppConstants.platform != "macosx") {
      return false;
    }
    return Services.appinfo.XPCOMABI.split("-")[0] == "x86";
  },

  /**
   * Checks whether or not a given plugin is visible in the addons manager
   * UI and the "enable DRM" notification box. This can be used to test
   * plugins that aren't yet turned on in the mozconfig.
   * @param   aPlugin
   *          The plugin to check.
   */
  _isPluginVisible(aPlugin) {
    return GMPPrefs.get(GMPPrefs.KEY_PLUGIN_VISIBLE, false, aPlugin.id);
  },

  /**
   * Checks whether or not a given plugin is forced-supported. This is used
   * in automated tests to override the checks that prevent GMPs running on an
   * unsupported platform.
   * @param   aPlugin
   *          The plugin to check.
   */
  _isPluginForceSupported(aPlugin) {
    return GMPPrefs.get(GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, false, aPlugin.id);
  },
};

/**
 * Manages preferences for GMP addons
 */
this.GMPPrefs = {
  KEY_EME_ENABLED:              "media.eme.enabled",
  KEY_PLUGIN_ENABLED:           "media.{0}.enabled",
  KEY_PLUGIN_LAST_UPDATE:       "media.{0}.lastUpdate",
  KEY_PLUGIN_VERSION:           "media.{0}.version",
  KEY_PLUGIN_AUTOUPDATE:        "media.{0}.autoupdate",
  KEY_PLUGIN_VISIBLE:           "media.{0}.visible",
  KEY_PLUGIN_ABI:               "media.{0}.abi",
  KEY_PLUGIN_FORCE_SUPPORTED:   "media.{0}.forceSupported",
  KEY_URL:                      "media.gmp-manager.url",
  KEY_URL_OVERRIDE:             "media.gmp-manager.url.override",
  KEY_CERT_CHECKATTRS:          "media.gmp-manager.cert.checkAttributes",
  KEY_CERT_REQUIREBUILTIN:      "media.gmp-manager.cert.requireBuiltIn",
  KEY_UPDATE_LAST_CHECK:        "media.gmp-manager.lastCheck",
  KEY_SECONDS_BETWEEN_CHECKS:   "media.gmp-manager.secondsBetweenChecks",
  KEY_UPDATE_ENABLED:           "media.gmp-manager.updateEnabled",
  KEY_APP_DISTRIBUTION:         "distribution.id",
  KEY_APP_DISTRIBUTION_VERSION: "distribution.version",
  KEY_BUILDID:                  "media.gmp-manager.buildID",
  KEY_CERTS_BRANCH:             "media.gmp-manager.certs.",
  KEY_PROVIDER_ENABLED:         "media.gmp-provider.enabled",
  KEY_LOG_BASE:                 "media.gmp.log.",
  KEY_LOGGING_LEVEL:            "media.gmp.log.level",
  KEY_LOGGING_DUMP:             "media.gmp.log.dump",

  /**
   * Obtains the specified preference in relation to the specified plugin.
   * @param aKey The preference key value to use.
   * @param aDefaultValue The default value if no preference exists.
   * @param aPlugin The plugin to scope the preference to.
   * @return The obtained preference value, or the defaultValue if none exists.
   */
  get(aKey, aDefaultValue, aPlugin) {
    if (aKey === this.KEY_APP_DISTRIBUTION ||
        aKey === this.KEY_APP_DISTRIBUTION_VERSION) {
      let prefValue = "default";
      try {
        prefValue = Services.prefs.getDefaultBranch(null).getCharPref(aKey);
      } catch (e) {
        // use default when pref not found
      }
      return prefValue;
    }
    return Preferences.get(this.getPrefKey(aKey, aPlugin), aDefaultValue);
  },

  /**
   * Sets the specified preference in relation to the specified plugin.
   * @param aKey The preference key value to use.
   * @param aVal The value to set.
   * @param aPlugin The plugin to scope the preference to.
   */
  set(aKey, aVal, aPlugin) {
    Preferences.set(this.getPrefKey(aKey, aPlugin), aVal);
  },

  /**
   * Checks whether or not the specified preference is set in relation to the
   * specified plugin.
   * @param aKey The preference key value to use.
   * @param aPlugin The plugin to scope the preference to.
   * @return true if the preference is set, false otherwise.
   */
  isSet(aKey, aPlugin) {
    return Preferences.isSet(this.getPrefKey(aKey, aPlugin));
  },

  /**
   * Resets the specified preference in relation to the specified plugin to its
   * default.
   * @param aKey The preference key value to use.
   * @param aPlugin The plugin to scope the preference to.
   */
  reset(aKey, aPlugin) {
    Preferences.reset(this.getPrefKey(aKey, aPlugin));
  },

  /**
   * Scopes the specified preference key to the specified plugin.
   * @param aKey The preference key value to use.
   * @param aPlugin The plugin to scope the preference to.
   * @return A preference key scoped to the specified plugin.
   */
  getPrefKey(aKey, aPlugin) {
    return aKey.replace("{0}", aPlugin || "");
  },
};
