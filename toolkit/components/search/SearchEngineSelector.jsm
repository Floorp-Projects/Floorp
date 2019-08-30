/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchEngineSelector"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
});

const EXT_SEARCH_PREFIX = "resource://search-extensions/";
const ENGINE_CONFIG_URL = `${EXT_SEARCH_PREFIX}engines.json`;
const USER_LOCALE = "$USER_LOCALE";

function log(str) {
  SearchUtils.log("SearchEngineSelector " + str + "\n");
}

/**
 * SearchEngineSelector parses the JSON configuration for
 * search engines and returns the applicable engines depending
 * on their region + locale.
 */
class SearchEngineSelector {
  /**
   * @param {string} url - Location of the configuration.
   */
  async init(url = ENGINE_CONFIG_URL) {
    this.configuration = await this.getEngineConfiguration(url);
  }

  /**
   * @param {string} url - Location of the configuration.
   */
  async getEngineConfiguration(url) {
    const response = await fetch(url);
    return (await response.json()).data;
  }

  /**
   * @param {string} region - Users region.
   * @param {string} locale - Users locale.
   * @returns {object} result - An object with "engines" field, a sorted
   *   list of engines and optionally "privateDefault" indicating the
   *   name of the engine which should be the default in private.
   */
  fetchEngineConfiguration(region, locale) {
    if (!region || !locale) {
      throw new Error("region and locale parameters required");
    }
    log(`fetchEngineConfiguration ${region}:${locale}`);
    let engines = [];
    for (let config of this.configuration) {
      const appliesTo = config.appliesTo || [];
      const applies = appliesTo.filter(section => {
        let included =
          "included" in section &&
          this._isInSection(region, locale, section.included);
        let excluded =
          "excluded" in section &&
          this._isInSection(region, locale, section.excluded);
        return included && !excluded;
      });

      let baseConfig = this._copyObject({}, config);

      // Loop through all the appliedTo sections that apply to
      // this configuration
      if (applies.length) {
        for (let section of applies) {
          this._copyObject(baseConfig, section);
        }

        if (baseConfig.webExtensionLocale == USER_LOCALE) {
          baseConfig.webExtensionLocale = locale;
        }

        engines.push(baseConfig);
      }
    }

    engines.sort(this._sort.bind(this));

    let privateEngine = engines.find(engine => {
      return (
        "defaultPrivate" in engine &&
        ["yes", "yes-if-no-other"].includes(engine.defaultPrivate)
      );
    });

    let result = { engines };

    if (privateEngine) {
      result.privateDefault = privateEngine.engineName;
    }

    if (SearchUtils.loggingEnabled) {
      log("fetchEngineConfiguration: " + JSON.stringify(result));
    }
    return result;
  }

  _sort(a, b) {
    return this._sortIndex(b) - this._sortIndex(a);
  }

  /**
   * Create an index order to ensure default (and backup default)
   * engines are ordered correctly.
   * @param {object} obj
   *   Object representing the engine configation.
   * @returns {integer}
   *  Number indicating how this engine should be sorted.
   */
  _sortIndex(obj) {
    let orderHint = obj.orderHint || 0;
    if (this._isDefault(obj)) {
      orderHint += 50000;
    }
    if ("privateDefault" in obj && obj.default === "yes") {
      orderHint += 40000;
    }
    if ("default" in obj && obj.default === "yes-if-no-other") {
      orderHint += 20000;
    }
    if ("privateDefault" in obj && obj.default === "yes-if-no-other") {
      orderHint += 10000;
    }
    return orderHint;
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
      target[key] = source[key];
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
    let inLocales = locales.matches && locales.matches.includes(locale);
    let inRegions = config.regions && config.regions.includes(region);
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
