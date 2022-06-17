/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
});

/**
 * AddonSearchEngine represents a search engine defined by an add-on.
 */
class AddonSearchEngine extends lazy.SearchEngine {
  /**
   * Creates a AddonSearchEngine.
   *
   * @param {object} options
   * @param {object} [options.details]
   *   An object that simulates the manifest object from a WebExtension.
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor({ isAppProvided, details, json } = {}) {
    super({
      loadPath:
        "[other]addEngineWithDetails:" +
        (details?.extensionID ?? json.extensionID ?? json._extensionID ?? null),
      isAppProvided,
    });

    if (details) {
      if (!details.extensionID) {
        throw Components.Exception(
          "Empty extensionID passed to _createAndAddEngine!",
          Cr.NS_ERROR_INVALID_ARG
        );
      }

      this.#initFromManifest(
        details.extensionID,
        details.extensionBaseURI,
        details.manifest,
        details.locale,
        details.config
      );
    } else {
      this._initWithJSON(json);
    }
  }

  /**
   * Update this engine based on new manifest, used during
   * webextension upgrades.
   *
   * @param {string} extensionID
   *   The WebExtension ID.
   * @param {string} extensionBaseURI
   *   The Base URI of the WebExtension.
   * @param {object} manifest
   *   An object representing the WebExtensions' manifest.
   * @param {string} locale
   *   The locale that is being used for the WebExtension.
   * @param {object} [configuration]
   *   The search engine configuration for application provided engines, that
   *   may be overriding some of the WebExtension's settings.
   */
  updateFromManifest(
    extensionID,
    extensionBaseURI,
    manifest,
    locale,
    configuration = {}
  ) {
    this._urls = [];
    this._iconMapObj = null;
    this.#initFromManifest(
      extensionID,
      extensionBaseURI,
      manifest,
      locale,
      configuration
    );
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Initializes the engine based on the manifest and other values.
   *
   * @param {string} extensionID
   *   The WebExtension ID.
   * @param {string} extensionBaseURI
   *   The Base URI of the WebExtension.
   * @param {object} manifest
   *   An object representing the WebExtensions' manifest.
   * @param {string} locale
   *   The locale that is being used for the WebExtension.
   * @param {object} [configuration]
   *   The search engine configuration for application provided engines, that
   *   may be overriding some of the WebExtension's settings.
   */
  #initFromManifest(
    extensionID,
    extensionBaseURI,
    manifest,
    locale,
    configuration = {}
  ) {
    let searchProvider = manifest.chrome_settings_overrides.search_provider;

    this._extensionID = extensionID;
    this._locale = locale;

    // We only set _telemetryId for app-provided engines. See also telemetryId
    // getter.
    if (this._isAppProvided) {
      if (configuration.telemetryId) {
        this._telemetryId = configuration.telemetryId;
      } else {
        let telemetryId = extensionID.split("@")[0];
        if (locale != lazy.SearchUtils.DEFAULT_TAG) {
          telemetryId += "-" + locale;
        }
        this._telemetryId = telemetryId;
      }
    }

    // Set the main icon URL for the engine.
    let iconURL = searchProvider.favicon_url;

    if (!iconURL) {
      iconURL =
        manifest.icons &&
        extensionBaseURI.resolve(
          lazy.ExtensionParent.IconDetails.getPreferredIcon(manifest.icons).icon
        );
    }

    // Record other icons that the WebExtension has.
    if (manifest.icons) {
      let iconList = Object.entries(manifest.icons).map(icon => {
        return {
          width: icon[0],
          height: icon[0],
          url: extensionBaseURI.resolve(icon[1]),
        };
      });
      for (let icon of iconList) {
        this._addIconToMap(icon.size, icon.size, icon.url);
      }
    }

    // Filter out any untranslated parameters, the extension has to list all
    // possible mozParams for each engine where a 'locale' may only provide
    // actual values for some (or none).
    if (searchProvider.params) {
      searchProvider.params = searchProvider.params.filter(param => {
        return !(param.value && param.value.startsWith("__MSG_"));
      });
    }

    this._initWithDetails(
      { ...searchProvider, iconURL, description: manifest.description },
      configuration
    );
  }
}

var EXPORTED_SYMBOLS = ["AddonSearchEngine"];
