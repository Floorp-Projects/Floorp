/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "ShellService",
  "resource:///modules/ShellService.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryArchive",
  "resource://gre/modules/TelemetryArchive.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FirstStartup",
  "resource://gre/modules/FirstStartup.jsm"
);

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
          if (
            mostRecentPings[ping.type].timestampCreated < ping.timestampCreated
          ) {
            mostRecentPings[ping.type] = ping;
          }
        } else {
          mostRecentPings[ping.type] = ping;
        }
      }

      const telemetry = {};
      for (const key in mostRecentPings) {
        const ping = mostRecentPings[key];
        telemetry[ping.type] = await TelemetryArchive.promiseArchivedPingById(
          ping.id
        );
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
    // Telemetry Environment is not available in early first-startup.
    if (FirstStartup.state === FirstStartup.IN_PROGRESS) {
      return undefined;
    }

    return (async () => {
      await TelemetryEnvironment.onInitialized();
      return TelemetryEnvironment.currentEnvironment.settings
        .defaultSearchEngine;
    })();
  }

  static get syncSetup() {
    return Services.prefs.prefHasUserValue("services.sync.username");
  }

  static get syncDesktopDevices() {
    return Services.prefs.getIntPref(
      "services.sync.clients.devices.desktop",
      0
    );
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
        const {
          id,
          isActive,
          name,
          type,
          version,
          installDate: installDateN,
        } = addon;
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
    return Services.locale.appLocaleAsLangTag;
  }

  static get doNotTrack() {
    return Services.prefs.getBoolPref(
      "privacy.donottrackheader.enabled",
      false
    );
  }

  static get os() {
    // Telemetry Environment is not available in early first-startup.
    if (FirstStartup.state === FirstStartup.IN_PROGRESS) {
      return undefined;
    }

    function coerceToNumber(version) {
      const parts = version.split(".");
      return parseFloat(parts.slice(0, 2).join("."));
    }

    return (async () => {
      await TelemetryEnvironment.onInitialized();

      const { system } = TelemetryEnvironment.currentEnvironment;
      const rv = {
        isWindows: AppConstants.platform === "win",
        isMac: AppConstants.platform === "macosx",
        isLinux: AppConstants.platform === "linux",
        windowsVersion: null,
        windowsBuildNumber: null,
        macVersion: null,
        darwinVersion: null,
      };

      if (rv.isWindows) {
        rv.windowsVersion = coerceToNumber(system.os.version);
        rv.windowsBuildNumber = system.os.windowsBuildNumber;
      } else if (rv.isMac) {
        rv.darwinVersion = coerceToNumber(system.os.version);
        // Versions of OSX with Darwin < 5 don't follow this pattern
        if (rv.darwinVersion >= 5) {
          // OSX 10.1 used Darwin 5, OSX 10.2 used Darwin 6, and so on.
          const intPart = Math.floor(rv.darwinVersion);
          rv.macVersion = 10 + 0.1 * (intPart - 4);
        }
      }

      // Version information on linux is a lot harder and a lot less useful, so
      // don't do anything about it here.

      return rv;
    })();
  }
}
