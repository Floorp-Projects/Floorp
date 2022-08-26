/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

const USER_LOCALE = "$USER_LOCALE";
const USER_REGION = "$USER_REGION";

XPCOMUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchEngineSelector",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

function hasAppKey(config, key) {
  return "application" in config && key in config.application;
}

function sectionExcludes(config, key, value) {
  return hasAppKey(config, key) && !config.application[key].includes(value);
}

function sectionIncludes(config, key, value) {
  return hasAppKey(config, key) && config.application[key].includes(value);
}

function isDistroExcluded(config, key, distroID) {
  // Should be excluded when:
  // - There's a distroID and that is not in the non-empty distroID list.
  // - There's no distroID and the distroID list is not empty.
  const appKey = hasAppKey(config, key);
  if (!appKey) {
    return false;
  }
  const distroList = config.application[key];
  if (distroID) {
    return distroList.length && !distroList.includes(distroID);
  }
  return !!distroList.length;
}

function belowMinVersion(config, version) {
  return (
    hasAppKey(config, "minVersion") &&
    Services.vc.compare(version, config.application.minVersion) < 0
  );
}

function aboveMaxVersion(config, version) {
  return (
    hasAppKey(config, "maxVersion") &&
    Services.vc.compare(version, config.application.maxVersion) > 0
  );
}

/**
 * SearchEngineSelector parses the JSON configuration for
 * search engines and returns the applicable engines depending
 * on their region + locale.
 */
export class SearchEngineSelector {
  /**
   * @param {function} listener
   *   A listener for configuration update changes.
   */
  constructor(listener) {
    this.QueryInterface = ChromeUtils.generateQI(["nsIObserver"]);
    this._remoteConfig = lazy.RemoteSettings(lazy.SearchUtils.SETTINGS_KEY);
    this._listenerAdded = false;
    this._onConfigurationUpdated = this._onConfigurationUpdated.bind(this);
    this._changeListener = listener;
  }

  /**
   * Handles getting the configuration from remote settings.
   */
  async getEngineConfiguration() {
    if (this._getConfigurationPromise) {
      return this._getConfigurationPromise;
    }

    this._configuration = await (this._getConfigurationPromise = this._getConfiguration());
    delete this._getConfigurationPromise;

    if (!this._configuration?.length) {
      throw Components.Exception(
        "Failed to get engine data from Remote Settings",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    if (!this._listenerAdded) {
      this._remoteConfig.on("sync", this._onConfigurationUpdated);
      this._listenerAdded = true;
    }

    return this._configuration;
  }

  /**
   * Obtains the configuration from remote settings. This includes
   * verifying the signature of the record within the database.
   *
   * If the signature in the database is invalid, the database will be wiped
   * and the stored dump will be used, until the settings next update.
   *
   * Note that this may cause a network check of the certificate, but that
   * should generally be quick.
   *
   * @param {boolean} [firstTime]
   *   Internal boolean to indicate if this is the first time check or not.
   * @returns {array}
   *   An array of objects in the database, or an empty array if none
   *   could be obtained.
   */
  async _getConfiguration(firstTime = true) {
    let result = [];
    let failed = false;
    try {
      result = await this._remoteConfig.get({
        order: "id",
      });
    } catch (ex) {
      lazy.logConsole.error(ex);
      failed = true;
    }
    if (!result.length) {
      lazy.logConsole.error("Received empty search configuration!");
      failed = true;
    }
    // If we failed, or the result is empty, try loading from the local dump.
    if (firstTime && failed) {
      await this._remoteConfig.db.clear();
      // Now call this again.
      return this._getConfiguration(false);
    }
    return result;
  }

  /**
   * Handles updating of the configuration. Note that the search service is
   * only updated after a period where the user is observed to be idle.
   */
  _onConfigurationUpdated({ data: { current } }) {
    this._configuration = current;
    lazy.logConsole.debug("Search configuration updated remotely");
    if (this._changeListener) {
      this._changeListener();
    }
  }

  /**
   * @param {object} options
   * @param {string} options.locale
   *   Users locale.
   * @param {string} options.region
   *   Users region.
   * @param {string} [options.channel]
   *   The update channel the application is running on.
   * @param {string} [options.distroID]
   *   The distribution ID of the application.
   * @param {string} [options.experiment]
   *   Any associated experiment id.
   * @param {string} [options.name]
   *   The name of the application.
   * @param {string} [options.version]
   *   The version of the application.
   * @returns {object}
   *   An object with "engines" field, a sorted list of engines and
   *   optionally "privateDefault" which is an object containing the engine
   *   details for the engine which should be the default in Private Browsing mode.
   */
  async fetchEngineConfiguration({
    locale,
    region,
    channel = "default",
    distroID,
    experiment,
    name = Services.appinfo.name ?? "",
    version = Services.appinfo.version ?? "",
  }) {
    if (!this._configuration) {
      await this.getEngineConfiguration();
    }
    lazy.logConsole.debug(
      `fetchEngineConfiguration ${locale}:${region}:${channel}:${distroID}:${experiment}:${name}:${version}`
    );
    let engines = [];
    const lcName = name.toLowerCase();
    const lcVersion = version.toLowerCase();
    const lcLocale = locale.toLowerCase();
    const lcRegion = region.toLowerCase();
    for (let config of this._configuration) {
      const appliesTo = config.appliesTo || [];
      const applies = appliesTo.filter(section => {
        if ("experiment" in section) {
          if (experiment != section.experiment) {
            return false;
          }
          if (section.override) {
            return true;
          }
        }

        let shouldInclude = () => {
          let included =
            "included" in section &&
            this._isInSection(lcRegion, lcLocale, section.included);
          let excluded =
            "excluded" in section &&
            this._isInSection(lcRegion, lcLocale, section.excluded);
          return included && !excluded;
        };

        const distroExcluded =
          (distroID &&
            sectionIncludes(section, "excludedDistributions", distroID)) ||
          isDistroExcluded(section, "distributions", distroID);

        if (distroID && !distroExcluded && section.override) {
          if ("included" in section || "excluded" in section) {
            return shouldInclude();
          }
          return true;
        }

        if (
          sectionExcludes(section, "channel", channel) ||
          sectionExcludes(section, "name", lcName) ||
          distroExcluded ||
          belowMinVersion(section, lcVersion) ||
          aboveMaxVersion(section, lcVersion)
        ) {
          return false;
        }
        return shouldInclude();
      });

      let baseConfig = this._copyObject({}, config);

      // Don't include any engines if every section is an override
      // entry, these are only supposed to override otherwise
      // included engine configurations.
      let allOverrides = applies.every(e => "override" in e && e.override);
      // Loop through all the appliedTo sections that apply to
      // this configuration.
      if (applies.length && !allOverrides) {
        for (let section of applies) {
          this._copyObject(baseConfig, section);
        }

        if (
          "webExtension" in baseConfig &&
          "locales" in baseConfig.webExtension
        ) {
          for (const webExtensionLocale of baseConfig.webExtension.locales) {
            const engine = { ...baseConfig };
            engine.webExtension = { ...baseConfig.webExtension };
            delete engine.webExtension.locales;
            engine.webExtension.locale = webExtensionLocale
              .replace(USER_LOCALE, locale)
              .replace(USER_REGION, lcRegion);
            engines.push(engine);
          }
        } else {
          const engine = { ...baseConfig };
          (engine.webExtension = engine.webExtension || {}).locale =
            lazy.SearchUtils.DEFAULT_TAG;
          engines.push(engine);
        }
      }
    }

    let defaultEngine;
    let privateEngine;

    function shouldPrefer(setting, hasCurrentDefault, currentDefaultSetting) {
      if (
        setting == "yes" &&
        (!hasCurrentDefault || currentDefaultSetting == "yes-if-no-other")
      ) {
        return true;
      }
      return setting == "yes-if-no-other" && !hasCurrentDefault;
    }

    for (const engine of engines) {
      engine.telemetryId = engine.telemetryId
        ?.replace(USER_LOCALE, locale)
        .replace(USER_REGION, lcRegion);
      if (
        "default" in engine &&
        shouldPrefer(
          engine.default,
          !!defaultEngine,
          defaultEngine && defaultEngine.default
        )
      ) {
        defaultEngine = engine;
      }
      if (
        "defaultPrivate" in engine &&
        shouldPrefer(
          engine.defaultPrivate,
          !!privateEngine,
          privateEngine && privateEngine.defaultPrivate
        )
      ) {
        privateEngine = engine;
      }
    }

    engines.sort(this._sort.bind(this, defaultEngine, privateEngine));

    let result = { engines };

    if (privateEngine) {
      result.privateDefault = privateEngine;
    }

    if (lazy.SearchUtils.loggingEnabled) {
      lazy.logConsole.debug(
        "fetchEngineConfiguration: " +
          result.engines.map(e => e.webExtension.id)
      );
    }
    return result;
  }

  _sort(defaultEngine, privateEngine, a, b) {
    return (
      this._sortIndex(b, defaultEngine, privateEngine) -
      this._sortIndex(a, defaultEngine, privateEngine)
    );
  }

  /**
   * Create an index order to ensure default (and backup default)
   * engines are ordered correctly.
   * @param {object} obj
   *   Object representing the engine configation.
   * @param {object} defaultEngine
   *   The default engine, for comparison to obj.
   * @param {object} privateEngine
   *   The private engine, for comparison to obj.
   * @returns {integer}
   *  Number indicating how this engine should be sorted.
   */
  _sortIndex(obj, defaultEngine, privateEngine) {
    if (obj == defaultEngine) {
      return Number.MAX_SAFE_INTEGER;
    }
    if (obj == privateEngine) {
      return Number.MAX_SAFE_INTEGER - 1;
    }
    return obj.orderHint || 0;
  }

  /**
   * Is the engine marked to be the default search engine.
   * @param {object} obj - Object representing the engine configation.
   * @returns {boolean} - Whether the engine should be default.
   */
  _isDefault(obj) {
    return "default" in obj && obj.default === "yes";
  }

  /**
   * Object.assign but ignore some keys
   * @param {object} target - Object to copy to.
   * @param {object} source - Object top copy from.
   * @returns {object} - The source object.
   */
  _copyObject(target, source) {
    for (let key in source) {
      if (["included", "excluded", "appliesTo"].includes(key)) {
        continue;
      }
      if (key == "webExtension") {
        if (key in target) {
          this._copyObject(target[key], source[key]);
        } else {
          target[key] = { ...source[key] };
        }
      } else {
        target[key] = source[key];
      }
    }
    return target;
  }

  /**
   * Determines wether the section of the config applies to a user
   * given what region + locale they are using.
   * @param {string} region - The region the user is in.
   * @param {string} locale - The language the user has configured.
   * @param {object} config - Section of configuration.
   * @returns {boolean} - Does the section apply for the region + locale.
   */
  _isInSection(region, locale, config) {
    if (!config) {
      return false;
    }
    if (config.everywhere) {
      return true;
    }
    let locales = config.locales || {};
    let inLocales =
      "matches" in locales &&
      !!locales.matches.find(e => e.toLowerCase() == locale);
    let inRegions =
      "regions" in config &&
      !!config.regions.find(e => e.toLowerCase() == region);
    if (
      locales.startsWith &&
      locales.startsWith.some(key => locale.startsWith(key))
    ) {
      inLocales = true;
    }
    if (config.locales && config.regions) {
      return inLocales && inRegions;
    }
    return inLocales || inRegions;
  }
}
