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
                          "OPEN_H264_ID" ];

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// GMP IDs
const OPEN_H264_ID  = "gmp-gmpopenh264";
const EME_ADOBE_ID  = "gmp-eme-adobe";
const GMP_PLUGIN_IDS = [ OPEN_H264_ID, EME_ADOBE_ID ];

let GMPPluginUnsupportedReason = {
  NOT_WINDOWS: 1,
  WINDOWS_VERSION: 2,
};

let GMPPluginHiddenReason = {
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
  isPluginHidden: function(aPlugin) {
    if (!aPlugin.isEME) {
      return false;
    }

    if (!this._isPluginSupported(aPlugin) &&
        !this._isPluginForcedVisible(aPlugin)) {
      this.maybeReportTelemetry(aPlugin.id,
                                "VIDEO_EME_ADOBE_HIDDEN_REASON",
                                GMPPluginHiddenReason.UNSUPPORTED);
      return true;
    }

    if (!GMPPrefs.get(GMPPrefs.KEY_EME_ENABLED, true)) {
      this.maybeReportTelemetry(aPlugin.id,
                                "VIDEO_EME_ADOBE_HIDDEN_REASON",
                                GMPPluginHiddenReason.EME_DISABLED);
      return true;
    }

    return false;
  },

  /**
   * Checks whether or not a given plugin is supported by the current OS.
   * @param   aPlugin
   *          The plugin to check.
   */
  _isPluginSupported: function(aPlugin) {
    if (aPlugin.id != EME_ADOBE_ID) {
      // Only checking Adobe EME at the moment.
      return true;
    }

    if (Services.appinfo.OS != "WINNT") {
      // Non-Windows OSes currently unsupported.
      this.maybeReportTelemetry(aPlugin.id,
                                "VIDEO_EME_ADOBE_UNSUPPORTED_REASON",
                                GMPPluginUnsupportedReason.NOT_WINDOWS);
      return false;
    }

    if (Services.sysinfo.getPropertyAsInt32("version") < 6) {
      // Windows versions before Vista are unsupported.
      this.maybeReportTelemetry(aPlugin.id,
                                "VIDEO_EME_ADOBE_UNSUPPORTED_REASON",
                                GMPPluginUnsupportedReason.WINDOWS_VERSION);
      return false;
    }

    return true;
  },

  /**
   * Checks whether or not a given plugin is forced visible. This can be used
   * to test plugins that aren't yet supported by default on a particular OS.
   * @param   aPlugin
   *          The plugin to check.
   */
  _isPluginForcedVisible: function(aPlugin) {
    return GMPPrefs.get(GMPPrefs.KEY_PLUGIN_FORCEVISIBLE, false, aPlugin.id);
  },

  /**
   * Report telemetry value, but only for Adobe CDM and only once per key
   * per session.
   */
  maybeReportTelemetry: function(aPluginId, key, value) {
    if (aPluginId != EME_ADOBE_ID) {
      // Only report for Adobe CDM.
      return;
    }

    if (!this.reportedKeys) {
      this.reportedKeys = [];
    }
    if (this.reportedKeys.indexOf(key) >= 0) {
      // Only report each key once per session.
      return;
    }
    this.reportedKeys.push(key);

    let hist = Services.telemetry.getHistogramById(key);
    if (hist) {
      hist.add(value);
    }
  }
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
  KEY_PLUGIN_FORCEVISIBLE:      "media.{0}.forcevisible",
  KEY_PLUGIN_TRIAL_CREATE:      "media.{0}.trial-create",
  KEY_URL:                      "media.gmp-manager.url",
  KEY_URL_OVERRIDE:             "media.gmp-manager.url.override",
  KEY_CERT_CHECKATTRS:          "media.gmp-manager.cert.checkAttributes",
  KEY_CERT_REQUIREBUILTIN:      "media.gmp-manager.cert.requireBuiltIn",
  KEY_UPDATE_LAST_CHECK:        "media.gmp-manager.lastCheck",
  KEY_SECONDS_BETWEEN_CHECKS:   "media.gmp-manager.secondsBetweenChecks",
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
  get: function(aKey, aDefaultValue, aPlugin) {
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
  set: function(aKey, aVal, aPlugin) {
    Preferences.set(this.getPrefKey(aKey, aPlugin), aVal);
  },

  /**
   * Checks whether or not the specified preference is set in relation to the
   * specified plugin.
   * @param aKey The preference key value to use.
   * @param aPlugin The plugin to scope the preference to.
   * @return true if the preference is set, false otherwise.
   */
  isSet: function(aKey, aPlugin) {
    return Preferences.isSet(this.getPrefKey(aKey, aPlugin));
  },

  /**
   * Resets the specified preference in relation to the specified plugin to its
   * default.
   * @param aKey The preference key value to use.
   * @param aPlugin The plugin to scope the preference to.
   */
  reset: function(aKey, aPlugin) {
    Preferences.reset(this.getPrefKey(aKey, aPlugin));
  },

  /**
   * Scopes the specified preference key to the specified plugin.
   * @param aKey The preference key value to use.
   * @param aPlugin The plugin to scope the preference to.
   * @return A preference key scoped to the specified plugin.
   */
  getPrefKey: function(aKey, aPlugin) {
    return aKey.replace("{0}", aPlugin || "");
  },
};
