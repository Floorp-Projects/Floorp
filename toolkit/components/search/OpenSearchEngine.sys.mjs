/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import {
  EngineURL,
  SearchEngine,
} from "resource://gre/modules/SearchEngine.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  loadAndParseOpenSearchEngine:
    "resource://gre/modules/OpenSearchLoader.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "OpenSearchEngine",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const OPENSEARCH_DEFAULT_UPDATE_INTERVAL = 7;

/**
 * OpenSearchEngine represents an OpenSearch base search engine.
 */
export class OpenSearchEngine extends SearchEngine {
  // The data describing the engine, in the form of an XML document element.
  _data = null;
  // The number of days between update checks for new versions
  _updateInterval = null;
  // The url to check at for a new update
  _updateURL = null;
  // The url to check for a new icon
  _iconUpdateURL = null;

  /**
   * Creates a OpenSearchEngine.
   *
   * @param {object} [options]
   *   The options object
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   * @param {OpenSearchProperties} [options.engineData]
   *   The engine data for this search engine that will have been loaded via
   *   `OpenSearchLoader`.
   */
  constructor(options = {}) {
    super({
      loadPath:
        options.json?._loadPath ??
        OpenSearchEngine.getAnonymizedLoadPath(
          lazy.SearchUtils.sanitizeName(options.engineData.name),
          options.engineData.installURL
        ),
    });

    if (options.engineData) {
      this.#setEngineData(options.engineData);

      // As this is a new engine, we must set the verification hash for the load
      // path set in the constructor.
      this.setAttr(
        "loadPathHash",
        lazy.SearchUtils.getVerificationHash(this._loadPath)
      );

      if (this.hasUpdates) {
        this.#setNextUpdateTime();
      }
    } else {
      this._initWithJSON(options.json);
      this._updateInterval = options.json._updateInterval ?? null;
      this._updateURL = options.json._updateURL ?? null;
      this._iconUpdateURL = options.json._iconUpdateURL ?? null;
    }
  }

  /**
   * Creates a JavaScript object that represents this engine.
   *
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    let json = super.toJSON();
    json._updateInterval = this._updateInterval;
    json._updateURL = this._updateURL;
    json._iconUpdateURL = this._iconUpdateURL;
    return json;
  }

  /**
   * Determines if this search engine has updates url.
   *
   * @returns {boolean}
   *   Returns true if this search engine may update itself.
   */
  get hasUpdates() {
    // Whether or not the engine has an update URL
    let selfURL = this._getURLOfType(
      lazy.SearchUtils.URL_TYPE.OPENSEARCH,
      "self"
    );
    return !!(this._updateURL || this._iconUpdateURL || selfURL);
  }

  /**
   * Returns the engine's updateURI if it exists and returns null otherwise
   *
   * @returns {?string}
   */
  get updateURI() {
    let updateURL = this._getURLOfType(lazy.SearchUtils.URL_TYPE.OPENSEARCH);
    let updateURI =
      updateURL && updateURL._hasRelation("self")
        ? updateURL.getSubmission("", this).uri
        : lazy.SearchUtils.makeURI(this._updateURL);
    return updateURI;
  }

  /**
   * Considers if this engine needs to be updated, and updates it if necessary.
   */
  async maybeUpdate() {
    if (!this.hasUpdates) {
      return;
    }

    let currentTime = Date.now();

    let expireTime = this.getAttr("updateexpir");

    if (!expireTime || !(expireTime <= currentTime)) {
      lazy.logConsole.debug(this.name, "Skipping update, not expired yet.");
      return;
    }

    await this.#update();

    this.#setNextUpdateTime();
  }

  /**
   * Updates the OpenSearch engine details from the server.
   */
  async #update() {
    let updateURI = this.updateURI;
    if (updateURI) {
      let data = await lazy.loadAndParseOpenSearchEngine(
        updateURI,
        this.getAttr("updatelastmodified")
      );

      this.#setEngineData(data);

      lazy.SearchUtils.notifyAction(
        this,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );

      // Keep track of the last modified date, so that we can make conditional
      // server requests for future updates.
      this.setAttr("updatelastmodified", new Date().toUTCString());
    }

    if (this._iconUpdateURL) {
      // Force update of the icon from the icon URL.
      this._setIcon(this._iconUpdateURL, true);
    }
  }

  /**
   * Sets the data for this engine based on the OpenSearch properties.
   *
   * @param {OpenSearchProperties} data
   *   The OpenSearch data.
   */
  #setEngineData(data) {
    let name = data.name.trim();
    if (!this._engineToUpdate) {
      if (Services.search.getEngineByName(name)) {
        throw Components.Exception(
          "Found a duplicate engine",
          Ci.nsISearchService.ERROR_DUPLICATE_ENGINE
        );
      }
    }

    this._name = name;
    this._description = data.description ?? "";
    this._searchForm = data.searchForm ?? "";
    this._queryCharset = data.queryCharset ?? "UTF-8";

    for (let url of data.urls) {
      let engineURL;
      try {
        engineURL = new EngineURL(url.type, url.method, url.template);
      } catch (ex) {
        throw Components.Exception(
          `Failed to add ${url.template} as an Engine URL`,
          Cr.NS_ERROR_FAILURE
        );
      }

      if (url.rels.length) {
        engineURL.rels = url.rels;
      }

      for (let param of url.params) {
        try {
          engineURL.addParam(param.name, param.value);
        } catch (ex) {
          // Ignore failure
          lazy.logConsole.error("OpenSearch url has an invalid param", param);
        }
      }

      this._urls.push(engineURL);
    }

    for (let image of data.images) {
      this._setIcon(image.url, image.isPrefered, image.width, image.height);
    }
  }

  /**
   * Sets the next update time for this engine.
   */
  #setNextUpdateTime() {
    var interval = this._updateInterval || OPENSEARCH_DEFAULT_UPDATE_INTERVAL;
    var milliseconds = interval * 86400000; // |interval| is in days
    this.setAttr("updateexpir", Date.now() + milliseconds);
  }

  /**
   * This indicates where we found the .xml file to load the engine,
   * and attempts to hide user-identifiable data (such as username).
   *
   * @param {string} sanitizedName
   *   The sanitized name of the engine.
   * @param {nsIURI} uri
   *   The uri the engine was loaded from.
   * @returns {string}
   *   A load path with reduced data.
   */
  static getAnonymizedLoadPath(sanitizedName, uri) {
    return `[${uri.scheme}]${uri.host}/${sanitizedName}.xml`;
  }
}
