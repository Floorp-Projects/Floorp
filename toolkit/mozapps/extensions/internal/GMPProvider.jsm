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
/* globals AddonManagerPrivate*/
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
/* globals OS*/
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/GMPUtils.jsm");
/* globals GMP_PLUGIN_IDS, GMPPrefs, GMPUtils, OPEN_H264_ID, WIDEVINE_ID */
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "GMPInstallManager", "resource://gre/modules/GMPInstallManager.jsm");
XPCOMUtils.defineLazyModuleGetter(
  this, "setTimeout", "resource://gre/modules/Timer.jsm");

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";

const SEC_IN_A_DAY           = 24 * 60 * 60;
// How long to wait after a user enabled EME before attempting to download CDMs.
const GMP_CHECK_DELAY        = 10 * 1000; // milliseconds

const NS_GRE_DIR             = "GreD";
const CLEARKEY_PLUGIN_ID     = "gmp-clearkey";
const CLEARKEY_VERSION       = "0.1";

const GMP_LICENSE_INFO       = "gmp_license_info";
const GMP_PRIVACY_INFO       = "gmp_privacy_info";
const GMP_LEARN_MORE         = "learn_more_label";

const GMP_PLUGINS = [
  {
    id:              OPEN_H264_ID,
    name:            "openH264_name",
    description:     "openH264_description2",
    // The following licenseURL is part of an awful hack to include the OpenH264
    // license without having bug 624602 fixed yet, and intentionally ignores
    // localisation.
    licenseURL:      "chrome://mozapps/content/extensions/OpenH264-license.txt",
    homepageURL:     "http://www.openh264.org/",
    optionsURL:      "chrome://mozapps/content/extensions/gmpPrefs.xul",
  },
  {
    id:              WIDEVINE_ID,
    name:            "widevine_description",
    // Describe the purpose of both CDMs in the same way.
    description:     "cdm_description",
    licenseURL:      "https://www.google.com/policies/privacy/",
    homepageURL:     "https://www.widevine.com/",
    optionsURL:      "chrome://mozapps/content/extensions/gmpPrefs.xul",
    isEME:           true
  }];
XPCOMUtils.defineConstant(this, "GMP_PLUGINS", GMP_PLUGINS);

XPCOMUtils.defineLazyGetter(this, "pluginsBundle",
  () => Services.strings.createBundle("chrome://global/locale/plugins.properties"));
XPCOMUtils.defineLazyGetter(this, "gmpService",
  () => Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(Ci.mozIGeckoMediaPluginChromeService));

var messageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                       .getService(Ci.nsIMessageListenerManager);

var gLogger;
var gLogAppenderDump = null;

function configureLogging() {
  if (!gLogger) {
    gLogger = Log.repository.getLogger("Toolkit.GMP");
    gLogger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
  }
  gLogger.level = GMPPrefs.getInt(GMPPrefs.KEY_LOGGING_LEVEL, Log.Level.Warn);

  let logDumping = GMPPrefs.getBool(GMPPrefs.KEY_LOGGING_DUMP, false);
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
 * The GMPWrapper provides the info for the various GMP plugins to public
 * callers through the API.
 */
function GMPWrapper(aPluginInfo) {
  this._plugin = aPluginInfo;
  this._log =
    Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP",
                                              "GMPWrapper(" +
                                              this._plugin.id + ") ");
  Services.prefs.addObserver(GMPPrefs.getPrefKey(GMPPrefs.KEY_PLUGIN_ENABLED,
                                                 this._plugin.id),
                             this, true);
  Services.prefs.addObserver(GMPPrefs.getPrefKey(GMPPrefs.KEY_PLUGIN_VERSION,
                                          this._plugin.id),
                             this, true);
  if (this._plugin.isEME) {
    Services.prefs.addObserver(GMPPrefs.KEY_EME_ENABLED, this, true);
    messageManager.addMessageListener("EMEVideo:ContentMediaKeysRequest", this);
  }
}

GMPWrapper.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  // An active task that checks for plugin updates and installs them.
  _updateTask: null,
  _gmpPath: null,
  _isUpdateCheckPending: false,

  optionsType: AddonManager.OPTIONS_TYPE_INLINE,
  get optionsURL() { return this._plugin.optionsURL; },

  set gmpPath(aPath) { this._gmpPath = aPath; },
  get gmpPath() {
    if (!this._gmpPath && this.isInstalled) {
      this._gmpPath = OS.Path.join(OS.Constants.Path.profileDir,
                                   this._plugin.id,
                                   GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION,
                                                      null, this._plugin.id));
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

  get version() {
    return GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION, null, this._plugin.id);
  },

  get isActive() {
    return !this.appDisabled &&
           !this.userDisabled &&
           !GMPUtils.isPluginHidden(this._plugin);
  },
  get appDisabled() {
    if (this._plugin.isEME && !GMPPrefs.getBool(GMPPrefs.KEY_EME_ENABLED, true)) {
      // If "media.eme.enabled" is false, all EME plugins are disabled.
      return true;
    }
    return false;
  },

  get userDisabled() {
    return !GMPPrefs.getBool(GMPPrefs.KEY_PLUGIN_ENABLED, true, this._plugin.id);
  },
  set userDisabled(aVal) {
    GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_ENABLED, aVal === false, this._plugin.id);
  },

  get blocklistState() { return Ci.nsIBlocklistService.STATE_NOT_BLOCKED; },
  get size() { return 0; },
  get scope() { return AddonManager.SCOPE_APPLICATION; },
  get pendingOperations() { return AddonManager.PENDING_NONE; },

  get operationsRequiringRestart() { return AddonManager.OP_NEEDS_RESTART_NONE; },

  get permissions() {
    let permissions = 0;
    if (!this.appDisabled) {
      permissions |= AddonManager.PERM_CAN_UPGRADE;
      permissions |= this.userDisabled ? AddonManager.PERM_CAN_ENABLE :
                                         AddonManager.PERM_CAN_DISABLE;
    }
    return permissions;
  },

  get updateDate() {
    let time = Number(GMPPrefs.getInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, 0,
                                      this._plugin.id));
    if (this.isInstalled) {
      return new Date(time * 1000);
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

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return true;
  },

  get applyBackgroundUpdates() {
    if (!GMPPrefs.isSet(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, this._plugin.id)) {
      return AddonManager.AUTOUPDATE_DEFAULT;
    }

    return GMPPrefs.getBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, this._plugin.id) ?
      AddonManager.AUTOUPDATE_ENABLE : AddonManager.AUTOUPDATE_DISABLE;
  },

  set applyBackgroundUpdates(aVal) {
    if (aVal == AddonManager.AUTOUPDATE_DEFAULT) {
      GMPPrefs.reset(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, this._plugin.id);
    } else if (aVal == AddonManager.AUTOUPDATE_ENABLE) {
      GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, this._plugin.id);
    } else if (aVal == AddonManager.AUTOUPDATE_DISABLE) {
      GMPPrefs.setBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, false, this._plugin.id);
    }
  },

  findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
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
        Date.now() / 1000 - Services.prefs.getIntPref(GMPPrefs.KEY_UPDATE_LAST_CHECK, 0);
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

    this._updateTask = (async () => {
      this._log.trace("findUpdates() - updateTask");
      try {
        let installManager = new GMPInstallManager();
        let res = await installManager.checkForAddons();
        let update = res.gmpAddons.find(addon => addon.id === this._plugin.id);
        if (update && update.isValid && !update.isInstalled) {
          this._log.trace("findUpdates() - found update for " +
                          this._plugin.id + ", installing");
          await installManager.installAddon(update);
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
      }
      return true;
    })();

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

  _handleEnabledChanged() {
    this._log.info("_handleEnabledChanged() id=" +
      this._plugin.id + " isActive=" + this.isActive);

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

  onPrefEMEGlobalEnabledChanged() {
    this._log.info("onPrefEMEGlobalEnabledChanged() id=" + this._plugin.id +
      " appDisabled=" + this.appDisabled + " isActive=" + this.isActive +
      " hidden=" + GMPUtils.isPluginHidden(this._plugin));

    AddonManagerPrivate.callAddonListeners("onPropertyChanged", this,
                                           ["appDisabled"]);
    // If EME or the GMP itself are disabled, uninstall the GMP.
    // Otherwise, check for updates, so we download and install the GMP.
    if (this.appDisabled) {
      this.uninstallPlugin();
    } else if (!GMPUtils.isPluginHidden(this._plugin)) {
      AddonManagerPrivate.callInstallListeners("onExternalInstall", null, this,
                                               null, false);
      AddonManagerPrivate.callAddonListeners("onInstalling", this, false);
      AddonManagerPrivate.callAddonListeners("onInstalled", this);
      this.checkForUpdates(GMP_CHECK_DELAY);
    }
    if (!this.userDisabled) {
      this._handleEnabledChanged();
    }
  },

  checkForUpdates(delay) {
    if (this._isUpdateCheckPending) {
      return;
    }
    this._isUpdateCheckPending = true;
    GMPPrefs.reset(GMPPrefs.KEY_UPDATE_LAST_CHECK, null);
    // Delay this in case the user changes his mind and doesn't want to
    // enable EME after all.
    setTimeout(() => {
      if (!this.appDisabled) {
        let gmpInstallManager = new GMPInstallManager();
        // We don't really care about the results, if someone is interested
        // they can check the log.
        gmpInstallManager.simpleCheckAndInstall().catch(() => {});
      }
      this._isUpdateCheckPending = false;
    }, delay);
  },

  receiveMessage({target: browser, data: data}) {
    this._log.trace("receiveMessage() data=" + data);
    let parsedData;
    try {
      parsedData = JSON.parse(data);
    } catch (ex) {
      this._log.error("Malformed EME video message with data: " + data);
      return;
    }
    let {status} = parsedData;
    if (status == "cdm-not-installed") {
      this.checkForUpdates(0);
    }
  },

  onPrefEnabledChanged() {
    if (!this._plugin.isEME || !this.appDisabled) {
      this._handleEnabledChanged();
    }
  },

  onPrefVersionChanged() {
    AddonManagerPrivate.callAddonListeners("onUninstalling", this, false);
    if (this._gmpPath) {
      this._log.info("onPrefVersionChanged() - unregistering gmp directory " +
                     this._gmpPath);
      gmpService.removeAndDeletePluginDirectory(this._gmpPath, true /* can defer */);
    }
    AddonManagerPrivate.callAddonListeners("onUninstalled", this);

    AddonManagerPrivate.callInstallListeners("onExternalInstall", null, this,
                                             null, false);
    AddonManagerPrivate.callAddonListeners("onInstalling", this, false);
    this._gmpPath = null;
    if (this.isInstalled) {
      this._gmpPath = OS.Path.join(OS.Constants.Path.profileDir,
                                   this._plugin.id,
                                   GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION,
                                                      null, this._plugin.id));
    }
    if (this._gmpPath && this.isActive) {
      this._log.info("onPrefVersionChanged() - registering gmp directory " +
                     this._gmpPath);
      gmpService.addPluginDirectory(this._gmpPath);
    }
    AddonManagerPrivate.callAddonListeners("onInstalled", this);
  },

  observe(subject, topic, pref) {
    if (topic == "nsPref:changed") {
      if (pref == GMPPrefs.getPrefKey(GMPPrefs.KEY_PLUGIN_ENABLED, this._plugin.id)) {
        this.onPrefEnabledChanged();
      } else if (pref == GMPPrefs.getPrefKey(GMPPrefs.KEY_PLUGIN_VERSION, this._plugin.id)) {
        this.onPrefVersionChanged();
      } else if (pref == GMPPrefs.KEY_EME_ENABLED) {
        this.onPrefEMEGlobalEnabledChanged();
      }
    }
  },

  uninstallPlugin() {
    AddonManagerPrivate.callAddonListeners("onUninstalling", this, false);
    if (this.gmpPath) {
      this._log.info("uninstallPlugin() - unregistering gmp directory " +
                     this.gmpPath);
      gmpService.removeAndDeletePluginDirectory(this.gmpPath);
    }
    GMPPrefs.reset(GMPPrefs.KEY_PLUGIN_VERSION, this.id);
    GMPPrefs.reset(GMPPrefs.KEY_PLUGIN_ABI, this.id);
    GMPPrefs.reset(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, this.id);
    AddonManagerPrivate.callAddonListeners("onUninstalled", this);
  },

  shutdown() {
    Services.prefs.removeObserver(GMPPrefs.getPrefKey(GMPPrefs.KEY_PLUGIN_ENABLED,
                                                      this._plugin.id), this);
    Services.prefs.removeObserver(GMPPrefs.getPrefKey(GMPPrefs.KEY_PLUGIN_VERSION,
                                                      this._plugin.id), this);
    if (this._plugin.isEME) {
      Services.prefs.removeObserver(GMPPrefs.KEY_EME_ENABLED, this);
      messageManager.removeMessageListener("EMEVideo:ContentMediaKeysRequest", this);
    }
    return this._updateTask;
  },

  _arePluginFilesOnDisk() {
    let fileExists = function(aGmpPath, aFileName) {
      let f = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      let path = OS.Path.join(aGmpPath, aFileName);
      f.initWithPath(path);
      return f.exists();
    };

    let id = this._plugin.id.substring(4);
    let libName = AppConstants.DLL_PREFIX + id + AppConstants.DLL_SUFFIX;
    let infoName;
    if (this._plugin.id == WIDEVINE_ID) {
      infoName = "manifest.json";
    } else {
      infoName = id + ".info";
    }

    return fileExists(this.gmpPath, libName) &&
           fileExists(this.gmpPath, infoName);
  },

  validate() {
    if (!this.isInstalled) {
      // Not installed -> Valid.
      return {
        installed: false,
        valid: true
      };
    }

    let abi = GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_ABI, UpdateUtils.ABI,
                                 this._plugin.id);
    if (abi != UpdateUtils.ABI) {
      // ABI doesn't match. Possibly this is a profile migrated across platforms
      // or from 32 -> 64 bit.
      return {
        installed: true,
        mismatchedABI: true,
        valid: false
      };
    }

    // Installed -> Check if files are missing.
    let filesOnDisk = this._arePluginFilesOnDisk();
    return {
      installed: true,
      valid: filesOnDisk
    };
  },
};

var GMPProvider = {
  get name() { return "GMPProvider"; },

  _plugins: null,

  startup() {
    configureLogging();
    this._log = Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP",
                                                          "GMPProvider.");
    this.buildPluginList();
    this.ensureProperCDMInstallState();

    Services.prefs.addObserver(GMPPrefs.KEY_LOG_BASE, configureLogging);

    for (let plugin of this._plugins.values()) {
      let wrapper = plugin.wrapper;
      let gmpPath = wrapper.gmpPath;
      let isEnabled = wrapper.isActive;
      this._log.trace("startup - enabled=" + isEnabled + ", gmpPath=" +
                      gmpPath);

      if (gmpPath && isEnabled) {
        let validation = wrapper.validate();
        if (validation.mismatchedABI) {
          this._log.info("startup - gmp " + plugin.id +
                         " mismatched ABI, uninstalling");
          wrapper.uninstallPlugin();
          continue;
        }
        if (!validation.valid) {
          this._log.info("startup - gmp " + plugin.id +
                         " invalid, uninstalling");
          wrapper.uninstallPlugin();
          continue;
        }
        this._log.info("startup - adding gmp directory " + gmpPath);
        try {
          gmpService.addPluginDirectory(gmpPath);
        } catch (e) {
          if (e.name != "NS_ERROR_NOT_AVAILABLE")
            throw e;
          this._log.warn("startup - adding gmp directory failed with " +
                         e.name + " - sandboxing not available?", e);
        }
      }
    }

    try {
      let greDir = Services.dirsvc.get(NS_GRE_DIR,
                                       Ci.nsIFile);
      let clearkeyPath = OS.Path.join(greDir.path,
                                      CLEARKEY_PLUGIN_ID,
                                      CLEARKEY_VERSION);
      this._log.info("startup - adding clearkey CDM directory " +
                     clearkeyPath);
      gmpService.addPluginDirectory(clearkeyPath);
    } catch (e) {
      this._log.warn("startup - adding clearkey CDM failed", e);
    }
  },

  shutdown() {
    this._log.trace("shutdown");
    Services.prefs.removeObserver(GMPPrefs.KEY_LOG_BASE, configureLogging);

    let shutdownTask = (async () => {
      this._log.trace("shutdown - shutdownTask");
      let shutdownSucceeded = true;

      for (let plugin of this._plugins.values()) {
        try {
          await plugin.wrapper.shutdown();
        } catch (e) {
          shutdownSucceeded = false;
        }
      }

      this._plugins = null;

      if (!shutdownSucceeded) {
        throw new Error("Shutdown failed");
      }
    })();

    return shutdownTask;
  },

  getAddonByID(aId, aCallback) {
    if (!this.isEnabled) {
      aCallback(null);
      return;
    }

    let plugin = this._plugins.get(aId);
    if (plugin && !GMPUtils.isPluginHidden(plugin)) {
      aCallback(plugin.wrapper);
    } else {
      aCallback(null);
    }
  },

  getAddonsByTypes(aTypes, aCallback) {
    if (!this.isEnabled ||
        (aTypes && aTypes.indexOf("plugin") < 0)) {
      aCallback([]);
      return;
    }

    let results = Array.from(this._plugins.values())
      .filter(p => !GMPUtils.isPluginHidden(p))
      .map(p => p.wrapper);

    aCallback(results);
  },

  get isEnabled() {
    return GMPPrefs.getBool(GMPPrefs.KEY_PROVIDER_ENABLED, false);
  },

  generateFullDescription(aPlugin) {
    let rv = [];
    for (let [urlProp, labelId] of [["learnMoreURL", GMP_LEARN_MORE],
                                    ["licenseURL", aPlugin.id == WIDEVINE_ID ?
                                     GMP_PRIVACY_INFO : GMP_LICENSE_INFO]]) {
      if (aPlugin[urlProp]) {
        let label = pluginsBundle.GetStringFromName(labelId);
        rv.push(`<xhtml:a href="${aPlugin[urlProp]}" target="_blank">${label}</xhtml:a>.`);
      }
    }
    return rv.length ? rv.join("<xhtml:br /><xhtml:br />") : undefined;
  },

  buildPluginList() {
    this._plugins = new Map();
    for (let aPlugin of GMP_PLUGINS) {
      let plugin = {
        id: aPlugin.id,
        name: pluginsBundle.GetStringFromName(aPlugin.name),
        description: pluginsBundle.GetStringFromName(aPlugin.description),
        homepageURL: aPlugin.homepageURL,
        optionsURL: aPlugin.optionsURL,
        wrapper: null,
        isEME: aPlugin.isEME,
      };
      plugin.fullDescription = this.generateFullDescription(aPlugin);
      plugin.wrapper = new GMPWrapper(plugin);
      this._plugins.set(plugin.id, plugin);
    }
  },

  ensureProperCDMInstallState() {
    if (!GMPPrefs.getBool(GMPPrefs.KEY_EME_ENABLED, true)) {
      for (let plugin of this._plugins.values()) {
        if (plugin.isEME && plugin.wrapper.isInstalled) {
          gmpService.addPluginDirectory(plugin.wrapper.gmpPath);
          plugin.wrapper.uninstallPlugin();
        }
      }
    }
  },
};

AddonManagerPrivate.registerProvider(GMPProvider, [
  new AddonManagerPrivate.AddonType("plugin", URI_EXTENSION_STRINGS,
                                    "type.plugin.name",
                                    AddonManager.VIEW_TYPE_LIST, 6000,
                                    AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE)
]);
