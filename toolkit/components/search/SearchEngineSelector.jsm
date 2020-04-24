/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchEngineSelector"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const USER_LOCALE = "$USER_LOCALE";

function log(str) {
  SearchUtils.log("SearchEngineSelector " + str + "\n");
}

function getAppInfo(key) {
  let value = null;
  try {
    // Services.appinfo is often null in tests.
    value = Services.appinfo[key].toLowerCase();
  } catch (e) {}
  return value;
}

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
class SearchEngineSelector {
  /**
   * @param {function} listener
   *   A listener for configuration update changes.
   */
  constructor(listener) {
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);
    this._remoteConfig = RemoteSettings(SearchUtils.SETTINGS_KEY);
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
    try {
      result = await this._remoteConfig.get();
    } catch (ex) {
      if (
        ex instanceof RemoteSettingsClient.InvalidSignatureError &&
        firstTime
      ) {
        // The local database is invalid, try and reset it.
        await this._remoteConfig.db.clear();
        // Now call this again.
        return this._getConfiguration(false);
      }
      // Don't throw an error just log it, just continue with no data, and hopefully
      // a sync will fix things later on.
      Cu.reportError(ex);
    }
    return result;
  }

  /**
   * Handles updating of the configuration. Note that the search service is
   * only updated after a period where the user is observed to be idle.
   */
  _onConfigurationUpdated({ data: { current } }) {
    this._configuration = current;

    if (this._changeListener) {
      this._changeListener();
    }
  }

  /**
   * @param {string} locale - Users locale.
   * @param {string} region - Users region.
   * @param {string} channel - The update channel the application is running on.
   * @param {string} distroID - The distribution ID of the application.
   * @returns {object}
   *   An object with "engines" field, a sorted list of engines and
   *   optionally "privateDefault" which is an object containing the engine
   *   details for the engine which should be the default in Private Browsing mode.
   */
  async fetchEngineConfiguration(locale, region, channel, distroID) {
    if (!this._configuration) {
      await this.getEngineConfiguration();
    }
    let cohort = Services.prefs.getCharPref("browser.search.cohort", null);
    let name = getAppInfo("name");
    let version = getAppInfo("version");
    log(
      `fetchEngineConfiguration ${region}:${locale}:${channel}:${distroID}:${cohort}:${name}:${version}`
    );
    let engines = [];
    const lcLocale = locale.toLowerCase();
    const lcRegion = region.toLowerCase();
    for (let config of this._configuration) {
      const appliesTo = config.appliesTo || [];
      const applies = appliesTo.filter(section => {
        if ("cohort" in section && cohort != section.cohort) {
          return false;
        }
        const distroExcluded =
          (distroID &&
            sectionIncludes(section, "excludedDistributions", distroID)) ||
          isDistroExcluded(section, "distributions", distroID);

        if (distroID && !distroExcluded && section.override) {
          return true;
        }

        if (
          sectionExcludes(section, "channel", channel) ||
          sectionExcludes(section, "name", name) ||
          distroExcluded ||
          belowMinVersion(section, version) ||
          aboveMaxVersion(section, version)
        ) {
          return false;
        }
        let included =
          "included" in section &&
          this._isInSection(lcRegion, lcLocale, section.included);
        let excluded =
          "excluded" in section &&
          this._isInSection(lcRegion, lcLocale, section.excluded);
        return included && !excluded;
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
            engine.webExtension.locale =
              webExtensionLocale == USER_LOCALE ? locale : webExtensionLocale;
            engines.push(engine);
          }
        } else {
          const engine = { ...baseConfig };
          (engine.webExtension = engine.webExtension || {}).locale =
            SearchUtils.DEFAULT_TAG;
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

    if (SearchUtils.loggingEnabled) {
      log(
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
