/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import { SearchEngine } from "resource://gre/modules/SearchEngine.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

/**
 * AppProvidedSearchEngine represents a search engine defined by the
 * search configuration.
 */
export class AppProvidedSearchEngine extends SearchEngine {
  /**
   * @param {object} options
   *   The options object
   * @param {object} [options.details]
   *   An object that simulates the engine object from the config.
   */
  constructor({ details } = {}) {
    let extensionId = details?.config.webExtension.id;
    let id = extensionId + details?.locale;

    super({
      loadPath: "[app]" + extensionId,
      isAppProvided: true,
      id,
    });

    this._extensionID = extensionId;
    this._locale = details?.locale;

    if (details) {
      if (!details.config.webExtension.id) {
        throw Components.Exception(
          "Empty extensionID passed to _createAndAddEngine!",
          Cr.NS_ERROR_INVALID_ARG
        );
      }

      this.#init(details.locale, details.config);
    }
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
   * @param {string} locale
   *   The locale that is being used for this engine.
   * @param {object} [config]
   *   The search engine configuration for application provided engines.
   */
  #init(locale, config) {
    let searchProvider;
    if (locale != "default") {
      searchProvider = config.webExtension.searchProvider[locale];
    } else if (locale == "default" && config.webExtension.default_locale) {
      searchProvider =
        config.webExtension.searchProvider[config.webExtension.default_locale];
    } else {
      searchProvider = config.webExtension;
    }

    // We only set _telemetryId for app-provided engines. See also telemetryId
    // getter.
    if (config.telemetryId) {
      this._telemetryId = config.telemetryId;
    } else {
      let telemetryId = this.id.split("@")[0];
      if (locale != lazy.SearchUtils.DEFAULT_TAG) {
        telemetryId += "-" + locale;
      }
      this._telemetryId = telemetryId;
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

    this._initWithDetails(
      {
        ...searchProvider,
        //iconURL,
        description: config.webExtension.description,
      },
      config
    );
  }
}
