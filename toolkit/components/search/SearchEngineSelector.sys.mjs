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
    this._remoteConfigOverrides = lazy.RemoteSettings(
      lazy.SearchUtils.NEW_SETTINGS_OVERRIDES_KEY
    );
    this._listenerAdded = false;
    this._onConfigurationUpdated = this._onConfigurationUpdated.bind(this);
    this._onConfigurationOverridesUpdated =
      this._onConfigurationOverridesUpdated.bind(this);
    this._changeListener = listener;
  }

  /**
   * Handles getting the configuration from remote settings.
   */
  async getEngineConfiguration() {
    if (this._getConfigurationPromise) {
      return this._getConfigurationPromise;
    }

    this._getConfigurationPromise = Promise.all([
      this._getConfiguration(),
      this._getConfigurationOverrides(),
    ]);
    let remoteSettingsData = await this._getConfigurationPromise;
    this._configuration = remoteSettingsData[0];
    this._configurationOverrides = remoteSettingsData[1];
    delete this._getConfigurationPromise;

    if (!this._configuration?.length) {
      throw Components.Exception(
        "Failed to get engine data from Remote Settings",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    if (!this._listenerAdded) {
      this._remoteConfig.on("sync", this._onConfigurationUpdated);
      this._remoteConfigOverrides.on(
        "sync",
        this._onConfigurationOverridesUpdated
      );
      this._listenerAdded = true;
    }

    return this._configuration;
  }

  /**
   * Used by tests to get the configuration overrides.
   */
  async getEngineConfigurationOverrides() {
    await this.getEngineConfiguration();
    return this._configurationOverrides;
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
  _onConfigurationOverridesUpdated({ data: { current } }) {
    this._configurationOverrides = current;
    lazy.logConsole.debug("Search configuration overrides updated remotely");
    if (this._changeListener) {
      this._changeListener();
    }
  }

  /**
   * Obtains the configuration overrides from remote settings.
   *
   * @returns {Array}
   *   An array of objects in the database, or an empty array if none
   *   could be obtained.
   */
  async _getConfigurationOverrides() {
    let result = [];
    try {
      result = await this._remoteConfigOverrides.get();
    } catch (ex) {
      // This data is remote only, so we just return an empty array if it fails.
    }
    return result;
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

    appName = appName.toLowerCase();
    version = version.toLowerCase();
    locale = locale.toLowerCase();
    region = region.toLowerCase();

    let engines = [];
    let defaultsConfig;
    let engineOrders;
    let userEnv = {
      appName,
      version,
      locale,
      region,
      channel,
      distroID,
      experiment,
    };

    for (let config of this._configuration) {
      if (config.recordType == "defaultEngines") {
        defaultsConfig = config;
      }

      if (config.recordType == "engineOrders") {
        engineOrders = config;
      }

      if (config.recordType !== "engine") {
        continue;
      }

      let variants =
        config.variants?.filter(variant =>
          this.#matchesUserEnvironment(variant, userEnv)
        ) ?? [];

      if (!variants.length) {
        continue;
      }

      let engine = structuredClone(config.base);
      engine.identifier = config.identifier;

      for (let variant of variants) {
        engine = this.#deepCopyObject(engine, variant);
      }

      for (let override of this._configurationOverrides) {
        if (override.identifier == engine.identifier) {
          engine = this.#deepCopyObject(engine, override);
        }
      }

      engines.push(engine);
    }

    let { defaultEngine, privateDefault } = this.#defaultEngines(
      engines,
      defaultsConfig,
      userEnv
    );

    for (const orderData of engineOrders.orders) {
      let environment = orderData.environment;

      if (this.#matchesUserEnvironment({ environment }, userEnv)) {
        this.#setEngineOrders(engines, orderData.order);
      }
    }

    engines.sort(this._sort.bind(this, defaultEngine, privateDefault));

    let result = { engines };

    if (privateDefault) {
      result.privateDefault = privateDefault;
    }

    if (lazy.SearchUtils.loggingEnabled) {
      lazy.logConsole.debug(
        "fetchEngineConfiguration: " + result.engines.map(e => e.identifier)
      );
    }
    return result;
  }

  _sort(defaultEngine, defaultPrivateEngine, a, b) {
    return (
      this._sortIndex(b, defaultEngine, defaultPrivateEngine) -
      this._sortIndex(a, defaultEngine, defaultPrivateEngine)
    );
  }

  /**
   * Create an index order to ensure default (and backup default)
   * engines are ordered correctly.
   *
   * @param {object} obj
   *   Object representing the engine configuration.
   * @param {object} defaultEngine
   *   The default engine, for comparison to obj.
   * @param {object} defaultPrivateEngine
   *   The default private engine, for comparison to obj.
   * @returns {integer}
   *  Number indicating how this engine should be sorted.
   */
  _sortIndex(obj, defaultEngine, defaultPrivateEngine) {
    if (obj == defaultEngine) {
      return Number.MAX_SAFE_INTEGER;
    }
    if (obj == defaultPrivateEngine) {
      return Number.MAX_SAFE_INTEGER - 1;
    }
    return obj.orderHint || 0;
  }

  /**
   * Deep copies an object to the target object and ignores some keys.
   *
   * @param {object} target - Object to copy to.
   * @param {object} source - Object to copy from.
   * @returns {object} - The source object.
   */
  #deepCopyObject(target, source) {
    for (let key in source) {
      if (["environment"].includes(key)) {
        continue;
      }

      if (typeof source[key] == "object" && !Array.isArray(source[key])) {
        if (key in target) {
          this.#deepCopyObject(target[key], source[key]);
        } else {
          target[key] = structuredClone(source[key]);
        }
      } else {
        target[key] = structuredClone(source[key]);
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

    if ("excludedDistributions" in config.environment) {
      if (config.environment.excludedDistributions.includes(user.distroID)) {
        return false;
      }
    }

    return (
      this.#matchesRegionAndLocale(
        user.region,
        user.locale,
        config.environment
      ) &&
      this.#matchesDistribution(
        user.distroID,
        config.environment.distributions
      ) &&
      this.#matchesVersions(
        config.environment.minVersion,
        config.environment.maxVersion,
        user.version
      ) &&
      this.#matchesChannel(config.environment.channels, user.channel) &&
      this.#matchesApplication(config.environment.applications, user.appName)
    );
  }

  /**
   * @param {string} userDistro
   *  The distribution from the user's environment.
   * @param {Array} configDistro
   *  An array of distributions for the particular environment in the config.
   * @returns {boolean}
   *  True if the user's distribution is included in the config distribution
   *  list.
   */
  #matchesDistribution(userDistro, configDistro) {
    // If there's no distribution for this engineConfig, ignore the check.
    if (!configDistro) {
      return true;
    }

    return configDistro?.includes(userDistro);
  }

  /**
   * @param {string} min
   *  The minimum version supported.
   * @param {string} max
   *  The maximum version supported.
   * @param {string} userVersion
   *  The user's version.
   * @returns {boolean}
   *  True if the user's version is within the range of the min and max versions
   *  supported.
   */
  #matchesVersions(min, max, userVersion) {
    // If there's no versions for this engineConfig, ignore the check.
    if (!min && !max) {
      return true;
    }

    if (!userVersion) {
      return false;
    }

    if (min && !max) {
      return this.#isAboveOrEqualMin(userVersion, min);
    }

    if (!min && max) {
      return this.#isBelowOrEqualMax(userVersion, max);
    }

    return (
      this.#isAboveOrEqualMin(userVersion, min) &&
      this.#isBelowOrEqualMax(userVersion, max)
    );
  }

  #isAboveOrEqualMin(userVersion, min) {
    return Services.vc.compare(userVersion, min) >= 0;
  }

  #isBelowOrEqualMax(userVersion, max) {
    return Services.vc.compare(userVersion, max) <= 0;
  }

  /**
   * @param {Array} configChannels
   *  Release channels such as nightly, beta, release, esr.
   * @param {string} userChannel
   *  The user's channel.
   * @returns {boolean}
   *  True if the user's channel is included in the config channels.
   */
  #matchesChannel(configChannels, userChannel) {
    // If there's no channels for this engineConfig, ignore the check.
    if (!configChannels) {
      return true;
    }

    return configChannels.includes(userChannel);
  }

  /**
   * @param {Array} configApps
   *  The applications such as firefox, firefox-android, firefox-ios,
   *  focus-android, and focus-ios.
   * @param {string} userApp
   *  The user's application.
   * @returns {boolean}
   *  True if the user's application is included in the config applications.
   */
  #matchesApplication(configApps, userApp) {
    // If there's no config Applications for this engineConfig, ignore the check.
    if (!configApps) {
      return true;
    }

    return configApps.includes(userApp);
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

    // When none of the regions and locales are set. This implies its available
    // everywhere.
    if (
      !Object.hasOwn(configEnv, "allRegionsAndLocales") &&
      !Object.hasOwn(configEnv, "regions") &&
      !Object.hasOwn(configEnv, "locales")
    ) {
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

  /**
   * Gets the default engine and default private engine based on the user's
   * environment.
   *
   * @param {Array} engines
   *   An array that contains the engines for the user environment.
   * @param {object} defaultsConfig
   *   The defaultEngines record type from the search config.
   * @param {object} userEnv
   *   The user's environment.
   * @returns {object}
   *   An object with default engine and default private engine.
   */
  #defaultEngines(engines, defaultsConfig, userEnv) {
    let defaultEngine, privateDefault;

    for (let data of defaultsConfig.specificDefaults) {
      let environment = data.environment;

      if (this.#matchesUserEnvironment({ environment }, userEnv)) {
        defaultEngine = this.#findDefault(engines, data) ?? defaultEngine;
        privateDefault =
          this.#findDefault(engines, data, "private") ?? privateDefault;
      }
    }

    defaultEngine ??= this.#findGlobalDefault(engines, defaultsConfig);
    privateDefault ??= this.#findGlobalDefault(
      engines,
      defaultsConfig,
      "private"
    );

    return { defaultEngine, privateDefault };
  }

  /**
   * Finds the global default engine or global default private engine.
   *
   * @param {Array} engines
   *   The engines for the user environment.
   * @param {string} config
   *   The defaultEngines record from the config.
   * @param {string} [engineType]
   *   A string to identify default or default private.
   * @returns {object}
   *   The global default engine or global default private engine.
   */
  #findGlobalDefault(engines, config, engineType = "default") {
    let engine;
    if (config.globalDefault && engineType == "default") {
      engine = engines.find(e => e.identifier == config.globalDefault);
    }

    if (config.globalDefaultPrivate && engineType == "private") {
      engine = engines.find(e => e.identifier == config.globalDefaultPrivate);
    }

    return engine;
  }

  /**
   * Finds the default engine or default private engine from the list of
   * engines that match the user's environment.
   *
   * @param {Array} engines
   *   The engines for the user environment.
   * @param {string} config
   *   The specific defaults record that contains the default engine or default
   *   private engine identifer for the environment.
   * @param {string} [engineType]
   *   A string to identify default engine or default private engine.
   * @returns {object|undefined}
   *   The default engine or default private engine. Undefined if none can be
   *   found.
   */
  #findDefault(engines, config, engineType = "default") {
    let defaultMatch =
      engineType == "default" ? config.default : config.defaultPrivate;

    if (!defaultMatch) {
      return undefined;
    }

    return this.#findEngineWithMatch(engines, defaultMatch);
  }

  /**
   * Sets the orderHint number for the engines.
   *
   * @param {Array} engines
   *  The engines for the user environment.
   * @param {Array} orderedEngines
   *  The ordering of engines. Engines in the beginning of the list get a higher
   *  orderHint number.
   */
  #setEngineOrders(engines, orderedEngines) {
    let orderNumber = orderedEngines.length;

    for (const engine of orderedEngines) {
      let foundEngine = this.#findEngineWithMatch(engines, engine);
      if (foundEngine) {
        foundEngine.orderHint = orderNumber;
        orderNumber -= 1;
      }
    }
  }

  /**
   * Finds an engine with the given match.
   *
   * @param {object[]} engines
   *   An array of search engine configurations.
   * @param {string} match
   *   A string to match against the engine identifier. This will be an exact
   *   match, unless the string ends with `*`, in which case it will use a
   *   startsWith match.
   * @returns {object|undefined}
   */
  #findEngineWithMatch(engines, match) {
    if (match.endsWith("*")) {
      let matchNoStar = match.slice(0, -1);
      return engines.find(e => e.identifier.startsWith(matchNoStar));
    }
    return engines.find(e => e.identifier == match);
  }
}
