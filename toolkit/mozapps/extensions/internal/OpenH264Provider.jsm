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

XPCOMUtils.defineLazyModuleGetter(this, "GMPInstallManager",
                                  "resource://gre/modules/GMPInstallManager.jsm");

const URI_EXTENSION_STRINGS    = "chrome://mozapps/locale/extensions/extensions.properties";
const STRING_TYPE_NAME         = "type.%ID%.name";

const SEC_IN_A_DAY              = 24 * 60 * 60;

const OPENH264_PLUGIN_ID       = "gmp-gmpopenh264";
const OPENH264_PREF_BRANCH     = "media." + OPENH264_PLUGIN_ID + ".";
const OPENH264_PREF_ENABLED    = "enabled";
const OPENH264_PREF_VERSION    = "version";
const OPENH264_PREF_LASTUPDATE = "lastUpdate";
const OPENH264_PREF_AUTOUPDATE = "autoupdate";
const OPENH264_PREF_PROVIDERENABLED = "provider.enabled";
const OPENH264_PREF_LOGGING    = "provider.logging";
const OPENH264_PREF_LOGGING_LEVEL = OPENH264_PREF_LOGGING + ".level"; // media.gmp-gmpopenh264.provider.logging.level
const OPENH264_PREF_LOGGING_DUMP = OPENH264_PREF_LOGGING + ".dump"; // media.gmp-gmpopenh264.provider.logging.dump
const OPENH264_HOMEPAGE_URL    = "http://www.openh264.org/";
const OPENH264_OPTIONS_URL     = "chrome://mozapps/content/extensions/openH264Prefs.xul";

const GMP_PREF_LASTCHECK       = "media.gmp-manager.lastCheck";

XPCOMUtils.defineLazyGetter(this, "pluginsBundle",
  () => Services.strings.createBundle("chrome://global/locale/plugins.properties"));
XPCOMUtils.defineLazyGetter(this, "prefs",
  () => new Preferences(OPENH264_PREF_BRANCH));
XPCOMUtils.defineLazyGetter(this, "gmpService",
  () => Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(Ci.mozIGeckoMediaPluginService));

let gLogger;
let gLogDumping = false;
let gLogAppenderDump = null;

function configureLogging() {
  if (!gLogger) {
    gLogger = Log.repository.getLogger("Toolkit.OpenH264Provider");
    gLogger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
  }
  gLogger.level = prefs.get(OPENH264_PREF_LOGGING_LEVEL, Log.Level.Warn);

  let logDumping = prefs.get(OPENH264_PREF_LOGGING_DUMP, false);
  if (logDumping != gLogDumping) {
    if (logDumping) {
      gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
      gLogger.addAppender(gLogAppenderDump);
    } else {
      gLogger.removeAppender(gLogAppenderDump);
      gLogAppenderDump = null;
    }
    gLogDumping = logDumping;
  }
}

/**
 * The OpenH264Wrapper provides the info for the OpenH264 GMP plugin to public callers through the API.
 */

let OpenH264Wrapper = {
  // An active task that checks for OpenH264 updates and installs them.
  _updateTask: null,

  _log: null,

  optionsType: AddonManager.OPTIONS_TYPE_INLINE,
  optionsURL: OPENH264_OPTIONS_URL,

  get id() { return OPENH264_PLUGIN_ID; },
  get type() { return "plugin"; },
  get name() { return pluginsBundle.GetStringFromName("openH264_name"); },
  get creator() { return null; },
  get homepageURL() { return OPENH264_HOMEPAGE_URL; },

  get description() { return pluginsBundle.GetStringFromName("openH264_description"); },

  get version() { return prefs.get(OPENH264_PREF_VERSION, ""); },

  get isActive() { return !this.userDisabled; },
  get appDisabled() { return false; },

  get userDisabled() { return !prefs.get(OPENH264_PREF_ENABLED, true); },
  set userDisabled(aVal) { prefs.set(OPENH264_PREF_ENABLED, aVal === false); },

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
    let time = Number(prefs.get(OPENH264_PREF_LASTUPDATE, null));
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
    if (!prefs.isSet(OPENH264_PREF_AUTOUPDATE)) {
      return AddonManager.AUTOUPDATE_DEFAULT;
    }

    return prefs.get(OPENH264_PREF_AUTOUPDATE, true) ?
      AddonManager.AUTOUPDATE_ENABLE : AddonManager.AUTOUPDATE_DISABLE;
  },

  set applyBackgroundUpdates(aVal) {
    if (aVal == AddonManager.AUTOUPDATE_DEFAULT) {
      prefs.reset(OPENH264_PREF_AUTOUPDATE);
    } else if (aVal == AddonManager.AUTOUPDATE_ENABLE) {
      prefs.set(OPENH264_PREF_AUTOUPDATE, true);
    } else if (aVal == AddonManager.AUTOUPDATE_DISABLE) {
      prefs.set(OPENH264_PREF_AUTOUPDATE, false);
    }
  },

  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
    this._log.trace("findUpdates() - reason=" + aReason);

    AddonManagerPrivate.callNoUpdateListeners(this, aListener);

    if (aReason === AddonManager.UPDATE_WHEN_PERIODIC_UPDATE) {
      if (!AddonManager.shouldAutoUpdate(this)) {
        this._log.trace("findUpdates() - no autoupdate");
        return Promise.resolve(false);
      }

      let secSinceLastCheck = Date.now() / 1000 - Preferences.get(GMP_PREF_LASTCHECK, 0);
      if (secSinceLastCheck <= SEC_IN_A_DAY) {
        this._log.trace("findUpdates() - last check was less then a day ago");
        return Promise.resolve(false);
      }
    } else if (aReason !== AddonManager.UPDATE_WHEN_USER_REQUESTED) {
      this._log.trace("findUpdates() - unsupported reason");
      return Promise.resolve(false);
    }

    if (this._updateTask !== null) {
      this._log.trace("findUpdates() - update task already running");
      return this._updateTask;
    }

    this._updateTask = Task.spawn(function* OpenH264Provider_updateTask() {
      this._log.trace("findUpdates() - updateTask");
      try {
        let installManager = new GMPInstallManager();
        let addons = yield installManager.checkForAddons();
        let openH264 = addons.find(addon => addon.isOpenH264);
        if (openH264 && !openH264.isInstalled) {
          this._log.trace("findUpdates() - found update, installing");
          yield installManager.installAddon(openH264);
        } else {
          this._log.trace("findUpdates() - no updates");
        }
        this._log.info("findUpdates() - updateTask succeeded");
      } catch (e) {
        this._log.error("findUpdates() - updateTask threw: " + e);
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
                              OPENH264_PLUGIN_ID,
                              this.version);
      return [path];
    }
    return [];
  },

  get isInstalled() {
    return this.version.length > 0;
  },
};

let OpenH264Provider = {
  startup: function() {
    configureLogging();
    this._log = Log.repository.getLoggerWithMessagePrefix("Toolkit.OpenH264Provider",
                                                          "OpenH264Provider" + "::");
    OpenH264Wrapper._log = Log.repository.getLoggerWithMessagePrefix("Toolkit.OpenH264Provider",
                                                                     "OpenH264Wrapper" + "::");
    this.gmpPath = null;
    if (OpenH264Wrapper.isInstalled) {
      this.gmpPath = OS.Path.join(OS.Constants.Path.profileDir,
                                  OPENH264_PLUGIN_ID,
                                  prefs.get(OPENH264_PREF_VERSION, null));
    }
    let enabled = prefs.get(OPENH264_PREF_ENABLED, true);
    this._log.trace("startup() - enabled=" + enabled + ", gmpPath="+this.gmpPath);


    Services.obs.addObserver(this, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED, false);
    prefs.observe(OPENH264_PREF_ENABLED, this.onPrefEnabledChanged, this);
    prefs.observe(OPENH264_PREF_VERSION, this.onPrefVersionChanged, this);
    prefs.observe(OPENH264_PREF_LOGGING, configureLogging);

    if (this.gmpPath && enabled) {
      this._log.info("startup() - adding gmp directory " + this.gmpPath);
      gmpService.addPluginDirectory(this.gmpPath);
    }
  },

  shutdown: function() {
    this._log.trace("shutdown()");
    Services.obs.removeObserver(this, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED);
    prefs.ignore(OPENH264_PREF_ENABLED, this.onPrefEnabledChanged, this);
    prefs.ignore(OPENH264_PREF_VERSION, this.onPrefVersionChanged, this);
    prefs.ignore(OPENH264_PREF_LOGGING, configureLogging);

    return OpenH264Wrapper._updateTask;
  },

  onPrefEnabledChanged: function() {
    let wrapper = OpenH264Wrapper;

    AddonManagerPrivate.callAddonListeners(wrapper.isActive ?
                                           "onEnabling" : "onDisabling",
                                           wrapper, false);
    if (this.gmpPath) {
      if (wrapper.isActive) {
        this._log.info("onPrefEnabledChanged() - adding gmp directory " + this.gmpPath);
        gmpService.addPluginDirectory(this.gmpPath);
      } else {
        this._log.info("onPrefEnabledChanged() - removing gmp directory " + this.gmpPath);
        gmpService.removePluginDirectory(this.gmpPath);
      }
    }
    AddonManagerPrivate.callAddonListeners(wrapper.isActive ?
                                           "onEnabled" : "onDisabled",
                                           wrapper);
  },

  onPrefVersionChanged: function() {
    let wrapper = OpenH264Wrapper;

    AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper, false);
    if (this.gmpPath) {
      this._log.info("onPrefVersionChanged() - unregistering gmp directory " + this.gmpPath);
      gmpService.removePluginDirectory(this.gmpPath);
    }
    AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);

    AddonManagerPrivate.callInstallListeners("onExternalInstall", null, wrapper, null, false);
    this.gmpPath = null;
    if (OpenH264Wrapper.isInstalled) {
      this.gmpPath = OS.Path.join(OS.Constants.Path.profileDir,
                                  OPENH264_PLUGIN_ID,
                                  prefs.get(OPENH264_PREF_VERSION, null));
    }
    if (this.gmpPath && wrapper.isActive) {
      this._log.info("onPrefVersionChanged() - registering gmp directory " + this.gmpPath);
      gmpService.addPluginDirectory(this.gmpPath);
    }
    AddonManagerPrivate.callAddonListeners("onInstalled", wrapper);
  },

  buildWrapper: function() {
    let description = pluginsBundle.GetStringFromName("openH264_description");
    return new OpenH264Wrapper(OPENH264_PLUGIN_ID,
                               OPENH264_PLUGIN_NAME,
                               description);
  },

  getAddonByID: function(aId, aCallback) {
    if (this.isEnabled && aId == OPENH264_PLUGIN_ID) {
      aCallback(OpenH264Wrapper);
    } else {
      aCallback(null);
    }
  },

  getAddonsByTypes: function(aTypes, aCallback) {
    if (!this.isEnabled || aTypes && aTypes.indexOf("plugin") < 0) {
      aCallback([]);
      return;
    }

    aCallback([OpenH264Wrapper]);
  },

  get isEnabled() {
    return prefs.get(OPENH264_PREF_PROVIDERENABLED, false);
  },
};

AddonManagerPrivate.registerProvider(OpenH264Provider, [
  new AddonManagerPrivate.AddonType("plugin", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 6000,
                                    AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE)
]);
