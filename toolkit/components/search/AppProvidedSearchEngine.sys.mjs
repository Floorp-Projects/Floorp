/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import {
  SearchEngine,
  EngineURL,
} from "resource://gre/modules/SearchEngine.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

/**
 * AppProvidedSearchEngine represents a search engine defined by the
 * search configuration.
 */
export class AppProvidedSearchEngine extends SearchEngine {
  static URL_TYPE_MAP = new Map([
    ["search", lazy.SearchUtils.URL_TYPE.SEARCH],
    ["suggestions", lazy.SearchUtils.URL_TYPE.SUGGEST_JSON],
    ["trending", lazy.SearchUtils.URL_TYPE.TRENDING_JSON],
  ]);

  /**
   * @param {object} config
   *   The engine config from Remote Settings.
   */
  constructor(config) {
    let extensionId = config.identifier;
    let id = config.identifier;

    super({
      loadPath: "[app]" + extensionId,
      isAppProvided: true,
      id,
    });

    this._extensionID = extensionId;
    this._locale = "default";

    this.#init(config);
  }

  /**
   * Update this engine based on new config, used during
   * config upgrades.

   * @param {object} options
   *   The options object.
   *
   * @param {object} options.locale
   *   The locale that is being used for the engine.
   * @param {object} options.configuration
   *   The search engine configuration for application provided engines.
   */
  update({ locale, configuration } = {}) {
    this._urls = [];
    this._iconMapObj = null;
    this.#init(locale, configuration);
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * This will update the application provided search engine if there is no
   * name change.
   *
   * @param {object} options
   *   The options object.
   * @param {object} [options.configuration]
   *   The search engine configuration for application provided engines.
   * @param {string} [options.locale]
   *   The locale to use for getting details of the search engine.
   * @returns {boolean}
   *   Returns true if the engine was updated, false otherwise.
   */
  async updateIfNoNameChange({ configuration, locale }) {
    let newName;
    if (locale != "default") {
      newName = configuration.webExtension.searchProvider[locale].name;
    } else if (
      locale == "default" &&
      configuration.webExtension.default_locale
    ) {
      newName =
        configuration.webExtension.searchProvider[
          configuration.webExtension.default_locale
        ].name;
    } else {
      newName = configuration.webExtension.name;
    }

    if (this.name != newName.trim()) {
      return false;
    }
    this.update({ locale, configuration });
    return true;
  }

  /**
   * Whether or not this engine is provided by the application, e.g. it is
   * in the list of configured search engines. Overrides the definition in
   * `SearchEngine`.
   *
   * @returns {boolean}
   */
  get isAppProvided() {
    return true;
  }

  /**
   * Whether or not this engine is an in-memory only search engine.
   * These engines are typically application provided or policy engines,
   * where they are loaded every time on SearchService initialization
   * using the policy JSON or the extension manifest. Minimal details of the
   * in-memory engines are saved to disk, but they are never loaded
   * from the user's saved settings file.
   *
   * @returns {boolean}
   *   Only returns true for application provided engines.
   */
  get inMemory() {
    return true;
  }

  get isGeneralPurposeEngine() {
    return !!(
      this._extensionID &&
      lazy.SearchUtils.GENERAL_SEARCH_ENGINE_IDS.has(this._extensionID)
    );
  }

  /**
   * Creates a JavaScript object that represents this engine.
   *
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    // For applicaiton provided engines we don't want to store all their data in
    // the settings file so just store the relevant metadata.
    return {
      id: this.id,
      _name: this.name,
      _isAppProvided: true,
      _metaData: this._metaData,
    };
  }

  /**
   * Initializes the engine.
   *
   * @param {object} [engineConfig]
   *   The search engine configuration for application provided engines.
   */
  #init(engineConfig) {
    this._orderHint = engineConfig.orderHint;
    this._telemetryId = engineConfig.identifier;

    if (engineConfig.telemetrySuffix) {
      this._telemetryId += `-${engineConfig.telemetrySuffix}`;
    }

    // Set the main icon URL for the engine.
    // let iconURL = searchProvider.favicon_url;

    // if (!iconURL) {
    //   iconURL =
    //     manifest.icons &&
    //     extensionBaseURI.resolve(
    //       lazy.ExtensionParent.IconDetails.getPreferredIcon(manifest.icons).icon
    //     );
    // }

    // // Record other icons that the WebExtension has.
    // if (manifest.icons) {
    //   let iconList = Object.entries(manifest.icons).map(icon => {
    //     return {
    //       width: icon[0],
    //       height: icon[0],
    //       url: extensionBaseURI.resolve(icon[1]),
    //     };
    //   });
    //   for (let icon of iconList) {
    //     this._addIconToMap(icon.size, icon.size, icon.url);
    //   }
    // }

    // this._initWithDetails(config);

    // this._sendAttributionRequest = config.sendAttributionRequest ?? false; // TODO check if we need to this?
    // if (details.iconURL) {
    //   this._setIcon(details.iconURL, true);
    // }

    this._name = engineConfig.name.trim();
    this._definedAliases =
      engineConfig.aliases?.map(alias => `@${alias}`) ?? [];

    for (const [type, urlData] of Object.entries(engineConfig.urls)) {
      this.#setUrl(type, urlData, engineConfig.partnerCode);
    }
  }

  /**
   * This sets the urls for the search engine based on the supplied parameters.
   *
   * @param {string} type
   *   The type of url. This could be a url for search, suggestions, or trending.
   * @param {object} urlData
   *   The url data contains the template/base url and url params.
   * @param {string} partnerCode
   *   The partner code associated with the search engine.
   */
  #setUrl(type, urlData, partnerCode) {
    let urlType = AppProvidedSearchEngine.URL_TYPE_MAP.get(type);

    if (!urlType) {
      console.warn("unexpected engine url type.", type);
      return;
    }

    let engineURL = new EngineURL(
      urlType,
      urlData.method || "GET",
      urlData.base
    );

    if (urlData.params) {
      for (const param of urlData.params) {
        switch (true) {
          case "value" in param:
            engineURL.addParam(
              param.name,
              param.value == "{partnerCode}" ? partnerCode : param.value
            );
            break;
          case "experimentConfig" in param:
            engineURL._addMozParam({
              name: param.name,
              pref: param.experimentConfig,
              condition: "pref",
            });
            break;
          case "searchAccessPoint" in param:
            for (const [key, value] of Object.entries(
              param.searchAccessPoint
            )) {
              engineURL.addParam(
                param.name,
                value,
                key == "addressbar" ? "keyword" : key
              );
            }
            break;
        }
      }
    }

    if (
      !("searchTermParamName" in urlData) &&
      !urlData.base.includes("{searchTerms}")
    ) {
      throw new Error("Search terms missing from engine URL.");
    }

    if ("searchTermParamName" in urlData) {
      // The search term parameter is always added last, which will add it to the
      // end of the URL. This is because in the past we have seen users trying to
      // modify their searches by altering the end of the URL.
      engineURL.addParam(urlData.searchTermParamName, "{searchTerms}");
    }

    this._urls.push(engineURL);
  }
}
