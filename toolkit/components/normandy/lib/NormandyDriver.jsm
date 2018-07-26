/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource:///modules/ShellService.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://normandy/lib/Addons.jsm");
ChromeUtils.import("resource://normandy/lib/LogManager.jsm");
ChromeUtils.import("resource://normandy/lib/Storage.jsm");
ChromeUtils.import("resource://normandy/lib/Heartbeat.jsm");
ChromeUtils.import("resource://normandy/lib/ClientEnvironment.jsm");
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm");

ChromeUtils.defineModuleGetter(
  this, "Sampling", "resource://gre/modules/components-utils/Sampling.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils", "resource://gre/modules/UpdateUtils.jsm");
ChromeUtils.defineModuleGetter(
  this, "AddonStudies", "resource://normandy/lib/AddonStudies.jsm");

const {generateUUID} = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

var EXPORTED_SYMBOLS = ["NormandyDriver"];

const log = LogManager.getLogger("normandy-driver");
const actionLog = LogManager.getLogger("normandy-driver.actions");

var NormandyDriver = function(sandboxManager) {
  if (!sandboxManager) {
    throw new Error("sandboxManager is required");
  }
  const {sandbox} = sandboxManager;

  return {
    testing: false,

    get locale() {
      if (Services.locale.getAppLocaleAsLangTag) {
        return Services.locale.getAppLocaleAsLangTag();
      }

      return Cc["@mozilla.org/chrome/chrome-registry;1"]
        .getService(Ci.nsIXULChromeRegistry)
        .getSelectedLocale("global");
    },

    get userId() {
      return ClientEnvironment.userId;
    },

    log(message, level = "debug") {
      const levels = ["debug", "info", "warn", "error"];
      if (!levels.includes(level)) {
        throw new Error(`Invalid log level "${level}"`);
      }
      actionLog[level](message);
    },

    showHeartbeat(options) {
      log.info(`Showing heartbeat prompt "${options.message}"`);
      const aWindow = Services.wm.getMostRecentWindow("navigator:browser");

      if (!aWindow) {
        return sandbox.Promise.reject(new sandbox.Error("No window to show heartbeat in"));
      }

      const internalOptions = Object.assign({}, options, {testing: this.testing});
      const heartbeat = new Heartbeat(aWindow, sandboxManager, internalOptions);
      return sandbox.Promise.resolve(heartbeat.eventEmitter.createSandboxedEmitter());
    },

    saveHeartbeatFlow() {
      // no-op required by spec
    },

    client() {
      const appinfo = {
        version: Services.appinfo.version,
        channel: UpdateUtils.getUpdateChannel(false),
        isDefaultBrowser: ShellService.isDefaultBrowser() || null,
        searchEngine: null,
        syncSetup: Preferences.isSet("services.sync.username"),
        syncDesktopDevices: Preferences.get("services.sync.clients.devices.desktop", 0),
        syncMobileDevices: Preferences.get("services.sync.clients.devices.mobile", 0),
        syncTotalDevices: null,
        plugins: {},
        doNotTrack: Preferences.get("privacy.donottrackheader.enabled", false),
        distribution: Preferences.get("distribution.id", "default"),
      };
      appinfo.syncTotalDevices = appinfo.syncDesktopDevices + appinfo.syncMobileDevices;

      const searchEnginePromise = new Promise(resolve => {
        Services.search.init(rv => {
          if (Components.isSuccessCode(rv)) {
            appinfo.searchEngine = Services.search.defaultEngine.identifier;
          }
          resolve();
        });
      });

      const pluginsPromise = (async () => {
        let plugins = await AddonManager.getAddonsByTypes(["plugin"]);
        plugins.forEach(plugin => appinfo.plugins[plugin.name] = {
          name: plugin.name,
          description: plugin.description,
          version: plugin.version,
        });
      })();

      return new sandbox.Promise(resolve => {
        Promise.all([searchEnginePromise, pluginsPromise]).then(() => {
          resolve(Cu.cloneInto(appinfo, sandbox));
        });
      });
    },

    uuid() {
      let ret = generateUUID().toString();
      ret = ret.slice(1, ret.length - 1);
      return ret;
    },

    createStorage(prefix) {
      const storage = new Storage(prefix);

      // Wrapped methods that we expose to the sandbox. These are documented in
      // the driver spec in docs/dev/driver.rst.
      const storageInterface = {};
      for (const method of ["getItem", "setItem", "removeItem", "clear"]) {
        storageInterface[method] = sandboxManager.wrapAsync(storage[method].bind(storage), {
          cloneArguments: true,
          cloneInto: true,
        });
      }

      return sandboxManager.cloneInto(storageInterface, {cloneFunctions: true});
    },

    setTimeout(cb, time) {
      if (typeof cb !== "function") {
        throw new sandbox.Error(`setTimeout must be called with a function, got "${typeof cb}"`);
      }
      const token = setTimeout(() => {
        cb();
        sandboxManager.removeHold(`setTimeout-${token}`);
      }, time);
      sandboxManager.addHold(`setTimeout-${token}`);
      return Cu.cloneInto(token, sandbox);
    },

    clearTimeout(token) {
      clearTimeout(token);
      sandboxManager.removeHold(`setTimeout-${token}`);
    },

    addons: {
      get: sandboxManager.wrapAsync(Addons.get.bind(Addons), {cloneInto: true}),
      install: sandboxManager.wrapAsync(Addons.install.bind(Addons)),
      uninstall: sandboxManager.wrapAsync(Addons.uninstall.bind(Addons)),
    },

    // Sampling
    ratioSample: sandboxManager.wrapAsync(Sampling.ratioSample),

    // Preference Experiment API
    preferenceExperiments: {
      start: sandboxManager.wrapAsync(
        PreferenceExperiments.start.bind(PreferenceExperiments),
        {cloneArguments: true}
      ),
      markLastSeen: sandboxManager.wrapAsync(
        PreferenceExperiments.markLastSeen.bind(PreferenceExperiments)
      ),
      stop: sandboxManager.wrapAsync(PreferenceExperiments.stop.bind(PreferenceExperiments)),
      get: sandboxManager.wrapAsync(
        PreferenceExperiments.get.bind(PreferenceExperiments),
        {cloneInto: true}
      ),
      getAllActive: sandboxManager.wrapAsync(
        PreferenceExperiments.getAllActive.bind(PreferenceExperiments),
        {cloneInto: true}
      ),
      has: sandboxManager.wrapAsync(PreferenceExperiments.has.bind(PreferenceExperiments)),
    },

    // Study storage API
    studies: {
      start: sandboxManager.wrapAsync(
        AddonStudies.start.bind(AddonStudies),
        {cloneArguments: true, cloneInto: true}
      ),
      stop: sandboxManager.wrapAsync(AddonStudies.stop.bind(AddonStudies)),
      get: sandboxManager.wrapAsync(AddonStudies.get.bind(AddonStudies), {cloneInto: true}),
      getAll: sandboxManager.wrapAsync(AddonStudies.getAll.bind(AddonStudies), {cloneInto: true}),
      has: sandboxManager.wrapAsync(AddonStudies.has.bind(AddonStudies)),
    },

    // Preference read-only API
    preferences: {
      getBool: wrapPrefGetter(Services.prefs.getBoolPref),
      getInt: wrapPrefGetter(Services.prefs.getIntPref),
      getChar: wrapPrefGetter(Services.prefs.getCharPref),
      has(name) {
        return Services.prefs.getPrefType(name) !== Services.prefs.PREF_INVALID;
      },
    },
  };
};

/**
 * Wrap a getter form nsIPrefBranch for use in the sandbox.
 *
 * We don't want to export the getters directly in case they add parameters that
 * aren't safe for the sandbox without us noticing; wrapping helps prevent
 * passing unknown parameters.
 *
 * @param {Function} getter
 *   Function on an nsIPrefBranch that fetches a preference value.
 * @return {Function}
 */
function wrapPrefGetter(getter) {
  return (value, defaultValue = undefined) => {
    // Passing undefined as the defaultValue disables throwing exceptions when
    // the pref is missing or the type doesn't match, so we need to specifically
    // exclude it if we don't want default value behavior.
    const args = [value];
    if (defaultValue !== undefined) {
      args.push(defaultValue);
    }
    return getter.apply(null, args);
  };
}
