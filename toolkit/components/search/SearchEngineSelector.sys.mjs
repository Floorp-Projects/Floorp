/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchEngineSelector",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

// function hasAppKey(config, key) {
//   return "application" in config && key in config.application;
// }

// function sectionExcludes(config, key, value) {
//   return hasAppKey(config, key) && !config.application[key].includes(value);
// }

// function sectionIncludes(config, key, value) {
//   return hasAppKey(config, key) && config.application[key].includes(value);
// }

// function isDistroExcluded(config, key, distroID) {
//   // Should be excluded when:
//   // - There's a distroID and that is not in the non-empty distroID list.
//   // - There's no distroID and the distroID list is not empty.
//   const appKey = hasAppKey(config, key);
//   if (!appKey) {
//     return false;
//   }
//   const distroList = config.application[key];
//   if (distroID) {
//     return distroList.length && !distroList.includes(distroID);
//   }
//   return !!distroList.length;
// }

// function belowMinVersion(config, version) {
//   return (
//     hasAppKey(config, "minVersion") &&
//     Services.vc.compare(version, config.application.minVersion) < 0
//   );
// }

// function aboveMaxVersion(config, version) {
//   return (
//     hasAppKey(config, "maxVersion") &&
//     Services.vc.compare(version, config.application.maxVersion) > 0
//   );
// }

/**
 * SearchEngineSelector parses the JSON configuration for
 * search engines and returns the applicable engines depending
 * on their region + locale.
 */
export class SearchEngineSelector {
  /**
   * @param {Function} listener
   *   A listener for configuration update changes.
   */
  constructor(listener) {
    this._remoteConfig = lazy.RemoteSettings(lazy.SearchUtils.NEW_SETTINGS_KEY);
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

    this._configuration = await (this._getConfigurationPromise =
      this._getConfiguration());
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
   * @returns {Array}
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
   *
   * @param {object} options
   *   The options object
   * @param {object} options.data
   *   The data to update
   * @param {Array} options.data.current
   *   The new configuration object
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
   *   The options object
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
   * @param {string} [options.appName]
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
    appName = Services.appinfo.name ?? "",
    version = Services.appinfo.version ?? "",
  }) {
    if (!this._configuration) {
      await this.getEngineConfiguration();
    }
    lazy.logConsole.debug(
      `fetchEngineConfiguration ${locale}:${region}:${channel}:${distroID}:${experiment}:${appName}:${version}`
    );
    let engines = [];

    appName = appName.toLowerCase();
    version = version.toLowerCase();
    locale = locale.toLowerCase();
    region = region.toLowerCase();

    for (let config of this._configuration) {
      if (config.recordType !== "engine") {
        continue;
      }

      if (
        this.#matchesUserEnvironment(config.base, {
          appName,
          version,
          locale,
          region,
          channel,
          distroID,
          experiment,
        })
      ) {
        let engine = this.#copyObject({}, config.base);
        engine.identifier = config.identifier;
        engines.push(engine);
      }
    }

    // let defaultEngine;
    // let privateEngine;

    // engines.sort(this._sort.bind(this, defaultEngine, privateEngine));

    let result = { engines };

    // if (privateEngine) {
    //   result.privateDefault = privateEngine;
    // }

    if (lazy.SearchUtils.loggingEnabled) {
      lazy.logConsole.debug(
        "fetchEngineConfiguration: " + result.engines.map(e => e.identifier)
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
   *
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
   *
   * @param {object} obj - Object representing the engine configation.
   * @returns {boolean} - Whether the engine should be default.
   */
  _isDefault(obj) {
    return "default" in obj && obj.default === "yes";
  }

  /**
   * Object.assign but ignore some keys
   *
   * @param {object} target - Object to copy to.
   * @param {object} source - Object top copy from.
   * @returns {object} - The source object.
   */
  #copyObject(target, source) {
    for (let sourceKey in source) {
      if (["environment"].includes(sourceKey)) {
        continue;
      }

      if (
        typeof source[sourceKey] == "object" &&
        !Array.isArray(source[sourceKey])
      ) {
        if (sourceKey in target) {
          this.#copyObject(target[sourceKey], source[sourceKey]);
        } else {
          target[sourceKey] = { ...source[sourceKey] };
        }
      } else {
        target[sourceKey] = source[sourceKey];
      }
    }
    return target;
  }

  /**
   * Matches the user's environment against the engine config's environment.
   *
   * @param {object} config
   *   The config for the given base or variant engine.
   * @param {object} user
   *   The user's environment we use to match with the engine's environment.
   * @param {string} user.appName
   *   The name of the application.
   * @param {string} user.version
   *   The version of the application.
   * @param {string} user.locale
   *   The locale of the user.
   * @param {string} user.region
   *   The region of the user.
   * @param {string} user.channel
   *   The channel the application is running on.
   * @param {string} user.distroID
   *   The distribution ID of the application.
   * @param {string} user.experiment
   *   Any associated experiment id.
   * @returns {boolean}
   *   True if the engine config's environment matches the user's environment.
   */
  #matchesUserEnvironment(config, user = {}) {
    if ("experiment" in config.environment) {
      if (user.experiment != config.environment.experiment) {
        return false;
      }
    }

    // const distroExcluded =
    //   (distroID &&
    //     sectionIncludes(section, "excludedDistributions", distroID)) ||
    //   isDistroExcluded(section, "distributions", distroID);
    // if (distroID && !distroExcluded && section.override) {
    //   if ("included" in section || "excluded" in section) {
    //     return shouldInclude();
    //   }
    //   return true;
    // }
    // if (
    //   sectionExcludes(section, "channel", channel) ||
    //   sectionExcludes(section, "name", lcAppName) ||
    //   distroExcluded ||
    //   belowMinVersion(section, lcVersion) ||
    //   aboveMaxVersion(section, lcVersion)
    // ) {
    //   return false;
    // }

    return this.#matchesRegionAndLocale(
      user.region,
      user.locale,
      config.environment
    );
  }

  /**
   * Determines whether the region and locale constraints in the config
   * environment  applies to a user given what region and locale they are using.
   *
   * @param {string} region
   *   The region the user is in.
   * @param {string} locale
   *   The language the user has configured.
   * @param {object} configEnv
   *   The environment of the engine configuration.
   * @returns {boolean}
   *   True if the user's region and locale matches the config's region and
   *   locale contraints. Otherwise false.
   */
  #matchesRegionAndLocale(region, locale, configEnv) {
    if (
      this.#doesConfigInclude(configEnv.excludedLocales, locale) ||
      this.#doesConfigInclude(configEnv.excludedRegions, region)
    ) {
      return false;
    }

    if (configEnv.allRegionsAndLocales) {
      return true;
    }

    if (
      this.#doesConfigInclude(configEnv?.locales, locale) &&
      this.#doesConfigInclude(configEnv?.regions, region)
    ) {
      return true;
    }

    if (
      this.#doesConfigInclude(configEnv?.locales, locale) &&
      !Object.hasOwn(configEnv, "regions")
    ) {
      return true;
    }

    if (
      this.#doesConfigInclude(configEnv?.regions, region) &&
      !Object.hasOwn(configEnv, "locales")
    ) {
      return true;
    }

    return false;
  }

  /**
   * This function converts the characters in the config to lowercase and
   * checks if the user's locale or region is included in config the
   * environment.
   *
   * @param {Array} configArray
   *   An Array of locales or regions from the config environment.
   * @param {string} compareItem
   *   The user's locale or region.
   * @returns {boolean}
   *   True if user's region or locale is found in the config environment.
   *   Otherwise false.
   */
  #doesConfigInclude(configArray, compareItem) {
    if (!configArray) {
      return false;
    }

    return configArray.find(
      configItem => configItem.toLowerCase() === compareItem
    );
  }
}
