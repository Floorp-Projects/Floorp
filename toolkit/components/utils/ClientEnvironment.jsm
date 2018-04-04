/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "ShellService", "resource:///modules/ShellService.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryArchive", "resource://gre/modules/TelemetryArchive.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils", "resource://gre/modules/UpdateUtils.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm");

var EXPORTED_SYMBOLS = ["ClientEnvironmentBase"];


/**
 * Create an object that provides general information about the client application.
 *
 * Components like Normandy RecipeRunner use this as part of the context for filter expressions,
 * so avoid adding non-getter functions as attributes, as filter expressions
 * cannot execute functions.
 *
 * Also note that, because filter expressions implicitly resolve promises, you
 * can add getter functions that return promises for async data.
 */
class ClientEnvironmentBase {
  static get distribution() {
    return Services.prefs.getCharPref("distribution.id", "default");
  }

  static get telemetry() {
    return (async () => {
      const pings = await TelemetryArchive.promiseArchivedPingList();

      // get most recent ping per type
      const mostRecentPings = {};
      for (const ping of pings) {
        if (ping.type in mostRecentPings) {
          if (mostRecentPings[ping.type].timestampCreated < ping.timestampCreated) {
            mostRecentPings[ping.type] = ping;
          }
        } else {
          mostRecentPings[ping.type] = ping;
        }
      }

      const telemetry = {};
      for (const key in mostRecentPings) {
        const ping = mostRecentPings[key];
        telemetry[ping.type] = await TelemetryArchive.promiseArchivedPingById(ping.id);
      }
      return telemetry;
    })();
  }

  static get version() {
    return AppConstants.MOZ_APP_VERSION_DISPLAY;
  }

  static get channel() {
    return UpdateUtils.getUpdateChannel(false);
  }

  static get isDefaultBrowser() {
    return ShellService.isDefaultBrowser();
  }

  static get searchEngine() {
    return (async () => {
      const searchInitialized = await new Promise(resolve => Services.search.init(resolve));
      if (Components.isSuccessCode(searchInitialized)) {
        return Services.search.defaultEngine.identifier;
      }
      return null;
    })();
  }

  static get syncSetup() {
    return Services.prefs.prefHasUserValue("services.sync.username");
  }

  static get syncDesktopDevices() {
    return Services.prefs.getIntPref("services.sync.clients.devices.desktop", 0);
  }

  static get syncMobileDevices() {
    return Services.prefs.getIntPref("services.sync.clients.devices.mobile", 0);
  }

  static get syncTotalDevices() {
    return this.syncDesktopDevices + this.syncMobileDevices;
  }

  static get addons() {
    return (async () => {
      const addons = await AddonManager.getAllAddons();
      return addons.reduce((acc, addon) => {
        const { id, isActive, name, type, version, installDate: installDateN } = addon;
        const installDate = new Date(installDateN);
        acc[id] = { id, isActive, name, type, version, installDate };
        return acc;
      }, {});
    })();
  }

  static get plugins() {
    return (async () => {
      const plugins = await AddonManager.getAddonsByTypes(["plugin"]);
      return plugins.reduce((acc, plugin) => {
        const { name, description, version } = plugin;
        acc[name] = { name, description, version };
        return acc;
      }, {});
    })();
  }

  static get locale() {
    return Services.locale.getAppLocaleAsLangTag();
  }

  static get doNotTrack() {
    return Services.prefs.getBoolPref("privacy.donottrackheader.enabled", false);
  }
}
