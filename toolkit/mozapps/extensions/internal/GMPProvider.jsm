/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "GMPInstallManager", "resource://gre/modules/GMPInstallManager.jsm");

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";
const STRING_TYPE_NAME       = "type.%ID%.name";

const SEC_IN_A_DAY           = 24 * 60 * 60;

const NS_GRE_DIR             = "GreD";
const CLEARKEY_PLUGIN_ID     = "gmp-clearkey";
const CLEARKEY_VERSION       = "0.1";

/**
 * Keys which can be used via GMPPrefs.
 */
const KEY_PROVIDER_ENABLED   = "media.gmp-provider.enabled";
const KEY_PROVIDER_LASTCHECK = "media.gmp-manager.lastCheck";
const KEY_LOG_BASE           = "media.gmp.log.";
const KEY_LOGGING_LEVEL      = KEY_LOG_BASE + "level";
const KEY_LOGGING_DUMP       = KEY_LOG_BASE + "dump";
const KEY_EME_ENABLED        = "media.eme.enabled"; // Global pref to enable/disable all EME plugins
const KEY_PLUGIN_ENABLED     = "media.{0}.enabled";
const KEY_PLUGIN_LAST_UPDATE = "media.{0}.lastUpdate";
const KEY_PLUGIN_VERSION     = "media.{0}.version";
const KEY_PLUGIN_AUTOUPDATE  = "media.{0}.autoupdate";
const KEY_PLUGIN_HIDDEN      = "media.{0}.hidden";

const GMP_LICENSE_INFO       = "gmp_license_info";

const GMP_PLUGINS = [
  {
    id:              "gmp-gmpopenh264",
    name:            "openH264_name",
    description:     "openH264_description",
    // The following licenseURL is part of an awful hack to include the OpenH264
    // license without having bug 624602 fixed yet, and intentionally ignores
    // localisation.
    licenseURL:      "chrome://mozapps/content/extensions/OpenH264-license.txt",
    homepageURL:     "http://www.openh264.org/",
    optionsURL:      "chrome://mozapps/content/extensions/gmpPrefs.xul"
  },
  {
    id:              "gmp-eme-adobe",
    name:            "eme-adobe_name",
    description:     "eme-adobe_description",
    licenseURL:      null,
    homepageURL:     "https://www.adobe.com/",
    optionsURL:      "chrome://mozapps/content/extensions/gmpPrefs.xul",
    isEME:           true
  }];

XPCOMUtils.defineLazyGetter(this, "pluginsBundle",
  () => Services.strings.createBundle("chrome://global/locale/plugins.properties"));
XPCOMUtils.defineLazyGetter(this, "gmpService",
  () => Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(Ci.mozIGeckoMediaPluginService));

let gLogger;
let gLogAppenderDump = null;

function configureLogging() {
  if (!gLogger) {
    gLogger = Log.repository.getLogger("Toolkit.GMP");
    gLogger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
  }
  gLogger.level = GMPPrefs.get(KEY_LOGGING_LEVEL, Log.Level.Warn);

  let logDumping = GMPPrefs.get(KEY_LOGGING_DUMP, false);
  if (logDumping != !!gLogAppenderDump) {
    if (logDumping) {
      gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
      gLogger.addAppender(gLogAppenderDump);
    } else {
      gLogger.removeAppender(gLogAppenderDump);
      gLogAppenderDump = null;
    }
  }
}

/**
 * Manages preferences for GMP addons
 */
let GMPPrefs = {
  /**
   * Obtains the specified preference in relation to the specified plugin.
   * @param aKey The preference key value to use.
   * @param aDefaultValue The default value if no preference exists.
   * @param aPlugin The plugin to scope the preference to.
   * @return The obtained preference value, or the defaultValue if none exists.
   */
  get: function(aKey, aDefaultValue, aPlugin) {
    return Preferences.get(this.getPrefKey(aKey, aPlugin), aDefaultValue);
  },
  /**
   * Sets the specified preference in relation to the specified plugin.
   * @param aKey The preference key value to use.
   * @param aVal The value to set.
   * @param aPlugin The plugin to scope the preference to.
   */
  set: function(aKey, aVal, aPlugin) {
    let log =
      Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP",
                                                "GMPProvider.jsm " +
                                                "GMPPrefs.set ");
    log.trace("Setting pref: " + this.getPrefKey(aKey, aPlugin) +
             " to value: " + aVal);
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
    return Preferences.isSet(GMPPrefs.getPrefKey(aKey, aPlugin));
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

/**
 * The GMPWrapper provides the info for the various GMP plugins to public
 * callers through the API.
 */
function GMPWrapper(aPluginInfo) {
  this._plugin = aPluginInfo;
  this._log =
    Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP",
                                              "GMPWrapper(" +
                                              this._plugin.id + ") ");
  Preferences.observe(GMPPrefs.getPrefKey(KEY_PLUGIN_ENABLED, this._plugin.id),
                      this.onPrefEnabledChanged, this);
  Preferences.observe(GMPPrefs.getPrefKey(KEY_PLUGIN_VERSION, this._plugin.id),
                      this.onPrefVersionChanged, this);
  if (this._plugin.isEME) {
    Preferences.observe(GMPPrefs.getPrefKey(KEY_EME_ENABLED, this._plugin.id),
                        this.onPrefEnabledChanged, this);
  }
}

GMPWrapper.prototype = {
  // An active task that checks for plugin updates and installs them.
  _updateTask: null,
  _gmpPath: null,

  optionsType: AddonManager.OPTIONS_TYPE_INLINE,
  get optionsURL() { return this._plugin.optionsURL; },

  set gmpPath(aPath) { this._gmpPath = aPath; },
  get gmpPath() {
    if (!this._gmpPath && this.isInstalled) {
      this._gmpPath = OS.Path.join(OS.Constants.Path.profileDir,
                                   this._plugin.id,
                                   GMPPrefs.get(KEY_PLUGIN_VERSION, null,
                                                this._plugin.id));
    }
    return this._gmpPath;
  },

  get id() { return this._plugin.id; },
  get type() { return "plugin"; },
  get isGMPlugin() { return true; },
  get name() { return this._plugin.name; },
  get creator() { return null; },
  get homepageURL() { return this._plugin.homepageURL; },

  get description() { return this._plugin.description; },
  get fullDescription() { return this._plugin.fullDescription; },

  get version() { return GMPPrefs.get(KEY_PLUGIN_VERSION, null,
                                      this._plugin.id); },

  get isActive() { return !this.userDisabled; },
  get appDisabled() { return false; },

  get userDisabled() {
    if (this._plugin.isEME && !GMPPrefs.get(KEY_EME_ENABLED, true)) {
      // If "media.eme.enabled" is false, all EME plugins are disabled.
      return true;
    }
    return !GMPPrefs.get(KEY_PLUGIN_ENABLED, true, this._plugin.id);
  },
  set userDisabled(aVal) { GMPPrefs.set(KEY_PLUGIN_ENABLED, aVal === false,
                                        this._plugin.id); },

  get blocklistState() { return Ci.nsIBlocklistService.STATE_NOT_BLOCKED; },
  get size() { return 0; },
  get scope() { return AddonManager.SCOPE_APPLICATION; },
  get pendingOperations() { return AddonManager.PENDING_NONE; },

  get operationsRequiringRestart() { return AddonManager.OP_NEEDS_RESTART_NONE },

  get permissions() {
    let permissions = AddonManager.PERM_CAN_UPGRADE;
    if (!this.appDisabled) {
      permissions |= this.userDisabled ? AddonManager.PERM_CAN_ENABLE :
                                         AddonManager.PERM_CAN_DISABLE;
    }
    return permissions;
  },

  get updateDate() {
    let time = Number(GMPPrefs.get(KEY_PLUGIN_LAST_UPDATE, null,
                                   this._plugin.id));
    if (time !== NaN && this.isInstalled) {
      return new Date(time * 1000)
    }
    return null;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  get foreignInstall() {
    return false;
  },

  isCompatibleWith: function(aAppVersion, aPlatformVersion) {
    return true;
  },

  get applyBackgroundUpdates() {
    if (!GMPPrefs.isSet(KEY_PLUGIN_AUTOUPDATE, this._plugin.id)) {
      return AddonManager.AUTOUPDATE_DEFAULT;
    }

    return GMPPrefs.get(KEY_PLUGIN_AUTOUPDATE, true, this._plugin.id) ?
      AddonManager.AUTOUPDATE_ENABLE : AddonManager.AUTOUPDATE_DISABLE;
  },

  set applyBackgroundUpdates(aVal) {
    if (aVal == AddonManager.AUTOUPDATE_DEFAULT) {
      GMPPrefs.reset(KEY_PLUGIN_AUTOUPDATE, this._plugin.id);
    } else if (aVal == AddonManager.AUTOUPDATE_ENABLE) {
      GMPPrefs.set(KEY_PLUGIN_AUTOUPDATE, true, this._plugin.id);
    } else if (aVal == AddonManager.AUTOUPDATE_DISABLE) {
      GMPPrefs.set(KEY_PLUGIN_AUTOUPDATE, false, this._plugin.id);
    }
  },

  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
    this._log.trace("findUpdates() - " + this._plugin.id + " - reason=" +
                    aReason);

    AddonManagerPrivate.callNoUpdateListeners(this, aListener);

    if (aReason === AddonManager.UPDATE_WHEN_PERIODIC_UPDATE) {
      if (!AddonManager.shouldAutoUpdate(this)) {
        this._log.trace("findUpdates() - " + this._plugin.id +
                        " - no autoupdate");
        return Promise.resolve(false);
      }

      let secSinceLastCheck =
        Date.now() / 1000 - Preferences.get(KEY_PROVIDER_LASTCHECK, 0);
      if (secSinceLastCheck <= SEC_IN_A_DAY) {
        this._log.trace("findUpdates() - " + this._plugin.id +
                        " - last check was less then a day ago");
        return Promise.resolve(false);
      }
    } else if (aReason !== AddonManager.UPDATE_WHEN_USER_REQUESTED) {
      this._log.trace("findUpdates() - " + this._plugin.id +
                      " - the given reason to update is not supported");
      return Promise.resolve(false);
    }

    if (this._updateTask !== null) {
      this._log.trace("findUpdates() - " + this._plugin.id +
                      " - update task already running");
      return this._updateTask;
    }

    this._updateTask = Task.spawn(function* GMPProvider_updateTask() {
      this._log.trace("findUpdates() - updateTask");
      try {
        let installManager = new GMPInstallManager();
        let gmpAddons = yield installManager.checkForAddons();
        let update = gmpAddons.find(function(aAddon) {
          return aAddon.id === this._plugin.id;
        }, this);
        if (update && update.isValid && !update.isInstalled) {
          this._log.trace("findUpdates() - found update for " +
                          this._plugin.id + ", installing");
          yield installManager.installAddon(update);
        } else {
          this._log.trace("findUpdates() - no updates for " + this._plugin.id);
        }
        this._log.info("findUpdates() - updateTask succeeded for " +
                       this._plugin.id);
      } catch (e) {
        this._log.error("findUpdates() - updateTask for " + this._plugin.id +
                        " threw", e);
        throw e;
      } finally {
        this._updateTask = null;
        return true;
      }
    }.bind(this));

    return this._updateTask;
  },

  get pluginMimeTypes() { return []; },
  get pluginLibraries() {
    if (this.isInstalled) {
      let path = this.version;
      return [path];
    }
    return [];
  },
  get pluginFullpath() {
    if (this.isInstalled) {
      let path = OS.Path.join(OS.Constants.Path.profileDir,
                              this._plugin.id,
                              this.version);
      return [path];
    }
    return [];
  },

  get isInstalled() {
    return this.version && this.version.length > 0;
  },

  onPrefEnabledChanged: function() {
    AddonManagerPrivate.callAddonListeners(this.isActive ?
                                           "onEnabling" : "onDisabling",
                                           this, false);
    if (this._gmpPath) {
      if (this.isActive) {
        this._log.info("onPrefEnabledChanged() - adding gmp directory " +
                       this._gmpPath);
        gmpService.addPluginDirectory(this._gmpPath);
      } else {
        this._log.info("onPrefEnabledChanged() - removing gmp directory " +
                       this._gmpPath);
        gmpService.removePluginDirectory(this._gmpPath);
      }
    }
    AddonManagerPrivate.callAddonListeners(this.isActive ?
                                           "onEnabled" : "onDisabled",
                                           this);
  },

  onPrefVersionChanged: function() {
    AddonManagerPrivate.callAddonListeners("onUninstalling", this, false);
    if (this._gmpPath) {
      this._log.info("onPrefVersionChanged() - unregistering gmp directory " +
                     this._gmpPath);
      gmpService.removePluginDirectory(this._gmpPath);
    }
    AddonManagerPrivate.callAddonListeners("onUninstalled", this);

    AddonManagerPrivate.callInstallListeners("onExternalInstall", null, this,
                                             null, false);
    this._gmpPath = null;
    if (this.isInstalled) {
      this._gmpPath = OS.Path.join(OS.Constants.Path.profileDir,
                                   this._plugin.id,
                                   GMPPrefs.get(KEY_PLUGIN_VERSION, null,
                                                this._plugin.id));
    }
    if (this._gmpPath && this.isActive) {
      this._log.info("onPrefVersionChanged() - registering gmp directory " +
                     this._gmpPath);
      gmpService.addPluginDirectory(this._gmpPath);
    }
    AddonManagerPrivate.callAddonListeners("onInstalled", this);
  },

  shutdown: function() {
    Preferences.ignore(GMPPrefs.getPrefKey(KEY_PLUGIN_ENABLED, this._plugin.id),
                       this.onPrefEnabledChanged, this);
    Preferences.ignore(GMPPrefs.getPrefKey(KEY_PLUGIN_VERSION, this._plugin.id),
                       this.onPrefVersionChanged, this);
    if (this._isEME) {
      Preferences.ignore(GMPPrefs.getPrefKey(KEY_EME_ENABLED, this._plugin.id),
                         this.onPrefEnabledChanged, this);
    }
    return this._updateTask;
  },
};

let GMPProvider = {
  get name() { return "GMPProvider"; },

  _plugins: null,

  startup: function() {
    configureLogging();
    this._log = Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP",
                                                          "GMPProvider.");
    let telemetry = {};
    this.buildPluginList();

    Preferences.observe(KEY_LOG_BASE, configureLogging);

    for (let [id, plugin] of this._plugins) {
      let wrapper = plugin.wrapper;
      let gmpPath = wrapper.gmpPath;
      let isEnabled = wrapper.isActive;
      this._log.trace("startup - enabled=" + isEnabled + ", gmpPath=" +
                      gmpPath);

      if (gmpPath && isEnabled) {
        this._log.info("startup - adding gmp directory " + gmpPath);
        try {
          gmpService.addPluginDirectory(gmpPath);
        } catch (e if e.name == 'NS_ERROR_NOT_AVAILABLE') {
          this._log.warn("startup - adding gmp directory failed with " +
                         e.name + " - sandboxing not available?", e);
        }
      }

      if (this.isEnabled) {
        telemetry[id] = {
          userDisabled: wrapper.userDisabled,
          version: wrapper.version,
          applyBackgroundUpdates: wrapper.applyBackgroundUpdates,
        };
      }
    }

    if (Preferences.get(KEY_EME_ENABLED, false)) {
      try {
        let greDir = Services.dirsvc.get(NS_GRE_DIR,
                                         Ci.nsILocalFile);
        let clearkeyPath = OS.Path.join(greDir.path,
                                        CLEARKEY_PLUGIN_ID,
                                        CLEARKEY_VERSION);
        this._log.info("startup - adding clearkey CDM directory " +
                       clearkeyPath);
        gmpService.addPluginDirectory(clearkeyPath);
      } catch (e) {
        this._log.warn("startup - adding clearkey CDM failed", e);
      }
    }

    AddonManagerPrivate.setTelemetryDetails("GMP", telemetry);
  },

  shutdown: function() {
    this._log.trace("shutdown");
    Preferences.ignore(KEY_LOG_BASE, configureLogging);

    let shutdownTask = Task.spawn(function* GMPProvider_shutdownTask() {
      this._log.trace("shutdown - shutdownTask");
      let shutdownSucceeded = true;

      for (let plugin of this._plugins.values()) {
        try {
          yield plugin.wrapper.shutdown();
        } catch (e) {
          shutdownSucceeded = false;
        }
      }

      this._plugins = null;

      if (!shutdownSucceeded) {
        throw new Error("Shutdown failed");
      }
    }.bind(this));

    return shutdownTask;
  },

  getAddonByID: function(aId, aCallback) {
    if (!this.isEnabled) {
      aCallback(null);
      return;
    }

    let plugin = this._plugins.get(aId);
    if (plugin) {
      aCallback(plugin.wrapper);
    } else {
      aCallback(null);
    }
  },

  getAddonsByTypes: function(aTypes, aCallback) {
    if (!this.isEnabled ||
        (aTypes && aTypes.indexOf("plugin") < 0)) {
      aCallback([]);
      return;
    }

    let results = [p.wrapper for ([id, p] of this._plugins)];
    aCallback(results);
  },

  get isEnabled() {
    return GMPPrefs.get(KEY_PROVIDER_ENABLED, false);
  },

  generateFullDescription: function(aLicenseURL, aLicenseInfo) {
    return "<xhtml:a href=\"" + aLicenseURL + "\" target=\"_blank\">" +
           aLicenseInfo + "</xhtml:a>."
  },

  buildPluginList: function() {
    let licenseInfo = pluginsBundle.GetStringFromName(GMP_LICENSE_INFO);

    let map = new Map();
    GMP_PLUGINS.forEach(aPlugin => {
      // Only show GMPs in addon manager that aren't hidden.
      if (!GMPPrefs.get(KEY_PLUGIN_HIDDEN, false, aPlugin.id)) {
        let plugin = {
          id: aPlugin.id,
          name: pluginsBundle.GetStringFromName(aPlugin.name),
          description: pluginsBundle.GetStringFromName(aPlugin.description),
          homepageURL: aPlugin.homepageURL,
          optionsURL: aPlugin.optionsURL,
          wrapper: null,
          isEME: aPlugin.isEME
        };
        if (aPlugin.licenseURL) {
          plugin.fullDescription =
            this.generateFullDescription(aPlugin.licenseURL, licenseInfo);
        }
        plugin.wrapper = new GMPWrapper(plugin);
        map.set(plugin.id, plugin);
      }
    }, this);
    this._plugins = map;
  },
};

AddonManagerPrivate.registerProvider(GMPProvider, [
  new AddonManagerPrivate.AddonType("plugin", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 6000,
                                    AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE)
]);
