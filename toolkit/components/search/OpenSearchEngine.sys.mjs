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
   * @param {boolean} [options.shouldPersist]
   *   A flag indicating whether the engine should be persisted to disk and made
   *   available wherever engines are used (e.g. it can be set as the default
   *   search engine, used for search shortcuts, etc.). Non-persisted engines
   *   are intended for more limited or temporary use. Defaults to true.
   */
  constructor(options = {}) {
    super({
      // We don't know what this is until after it has loaded, so add a placeholder.
      loadPath: options.json?._loadPath ?? "[opensearch]loading",
    });

    if (options.json) {
      this._initWithJSON(options.json);
      this._updateInterval = options.json._updateInterval ?? null;
      this._updateURL = options.json._updateURL ?? null;
      this._iconUpdateURL = options.json._iconUpdateURL ?? null;
    }

    this._shouldPersist = options.shouldPersist ?? true;
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
   * Retrieves the engine data from a URI. Initializes the engine, flushes to
   * disk, and notifies the search service once initialization is complete.
   *
   * @param {string|nsIURI} uri
   *   The uri to load the search plugin from.
   */
  async install(uri) {
    let loadURI =
      uri instanceof Ci.nsIURI ? uri : lazy.SearchUtils.makeURI(uri);
    let data = await lazy.loadAndParseOpenSearchEngine(
      loadURI,
      this._engineToUpdate?.getAttr("updatelastmodified")
    );

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

    if (this._engineToUpdate) {
      let engineToUpdate = this._engineToUpdate.wrappedJSObject;

      // Preserve metadata and loadPath.
      Object.keys(engineToUpdate._metaData).forEach(key => {
        this.setAttr(key, engineToUpdate.getAttr(key));
      });
      this._loadPath = engineToUpdate._loadPath;

      // Keep track of the last modified date, so that we can make conditional
      // requests for future updates.
      this.setAttr("updatelastmodified", new Date().toUTCString());

      // Set the new engine's icon, if it doesn't yet have one.
      if (!this._iconURI && engineToUpdate._iconURI) {
        this._iconURI = engineToUpdate._iconURI;
      }
    } else {
      this._loadPath = OpenSearchEngine.getAnonymizedLoadPath(
        lazy.SearchUtils.sanitizeName(this.name),
        loadURI
      );
      this.setAttr(
        "loadPathHash",
        lazy.SearchUtils.getVerificationHash(this._loadPath)
      );
    }

    for (let image of data.images) {
      this._setIcon(image.url, image.isPrefered, image.width, image.height);
    }

    if (this._shouldPersist) {
      // Notify the search service of the successful load. It will deal with
      // updates by checking this._engineToUpdate.
      lazy.SearchUtils.notifyAction(
        this,
        lazy.SearchUtils.MODIFIED_TYPE.LOADED
      );
    }
  }

  get _hasUpdates() {
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
   * @returns {string?}
   */
  get _updateURI() {
    let updateURL = this._getURLOfType(lazy.SearchUtils.URL_TYPE.OPENSEARCH);
    let updateURI =
      updateURL && updateURL._hasRelation("self")
        ? updateURL.getSubmission("", this).uri
        : lazy.SearchUtils.makeURI(this._updateURL);
    return updateURI;
  }

  // This indicates where we found the .xml file to load the engine,
  // and attempts to hide user-identifiable data (such as username).
  static getAnonymizedLoadPath(shortName, uri) {
    return `[${uri.scheme}]${uri.host}/${shortName}.xml`;
  }
}
