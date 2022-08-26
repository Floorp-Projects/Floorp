/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import {
  EngineURL,
  SearchEngine,
} from "resource://gre/modules/SearchEngine.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "OpenSearchEngine",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

const OPENSEARCH_NS_10 = "http://a9.com/-/spec/opensearch/1.0/";
const OPENSEARCH_NS_11 = "http://a9.com/-/spec/opensearch/1.1/";

// Although the specification at http://opensearch.a9.com/spec/1.1/description/
// gives the namespace names defined above, many existing OpenSearch engines
// are using the following versions.  We therefore allow either.
const OPENSEARCH_NAMESPACES = [
  OPENSEARCH_NS_11,
  OPENSEARCH_NS_10,
  "http://a9.com/-/spec/opensearchdescription/1.1/",
  "http://a9.com/-/spec/opensearchdescription/1.0/",
];

const OPENSEARCH_LOCALNAME = "OpenSearchDescription";

const MOZSEARCH_NS_10 = "http://www.mozilla.org/2006/browser/search/";
const MOZSEARCH_LOCALNAME = "SearchPlugin";

/**
 * Ensures an assertion is met before continuing. Should be used to indicate
 * fatal errors.
 * @param {*} assertion
 *   An assertion that must be met
 * @param {string} message
 *   A message to display if the assertion is not met
 * @param {number} resultCode
 *   The NS_ERROR_* value to throw if the assertion is not met
 * @throws resultCode
 *   If the assertion fails.
 */
function ENSURE_WARN(assertion, message, resultCode) {
  if (!assertion) {
    throw Components.Exception(message, resultCode);
  }
}

/**
 * OpenSearchEngine represents an OpenSearch base search engine.
 */
export class OpenSearchEngine extends SearchEngine {
  // The data describing the engine, in the form of an XML document element.
  _data = null;

  /**
   * Creates a OpenSearchEngine.
   *
   * @param {object} [options]
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor(options = {}) {
    super({
      // We don't know what this is until after it has loaded, so add a placeholder.
      loadPath: options.json?._loadPath ?? "[opensearch]loading",
    });

    if (options.json) {
      this._initWithJSON(options.json);
    }
  }

  /**
   * Retrieves the engine data from a URI. Initializes the engine, flushes to
   * disk, and notifies the search service once initialization is complete.
   *
   * @param {string|nsIURI} uri
   *   The uri to load the search plugin from.
   * @param {function} [callback]
   *   A callback to receive any details of errors.
   */
  install(uri, callback) {
    let loadURI =
      uri instanceof Ci.nsIURI ? uri : lazy.SearchUtils.makeURI(uri);
    if (!loadURI) {
      throw Components.Exception(
        loadURI,
        "Must have URI when calling _install!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }
    if (!/^https?$/i.test(loadURI.scheme)) {
      throw Components.Exception(
        "Invalid URI passed to SearchEngine constructor",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    lazy.logConsole.debug("_install: Downloading engine from:", loadURI.spec);

    var chan = lazy.SearchUtils.makeChannel(loadURI);

    if (this._engineToUpdate && chan instanceof Ci.nsIHttpChannel) {
      var lastModified = this._engineToUpdate.getAttr("updatelastmodified");
      if (lastModified) {
        chan.setRequestHeader("If-Modified-Since", lastModified, false);
      }
    }
    this._uri = loadURI;

    var listener = new lazy.SearchUtils.LoadListener(
      chan,
      /(^text\/|xml$)/,
      this._onLoad.bind(this, callback)
    );
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener);
  }

  /**
   * Handle the successful download of an engine. Initializes the engine and
   * triggers parsing of the data. The engine is then flushed to disk. Notifies
   * the search service once initialization is complete.
   *
   * @param {function} callback
   *   A callback to receive success or failure notifications. May be null.
   * @param {array} bytes
   *  The loaded search engine data.
   */
  _onLoad(callback, bytes) {
    let onError = errorCode => {
      if (this._engineToUpdate) {
        lazy.logConsole.warn("Failed to update", this._engineToUpdate.name);
      }
      callback?.(errorCode);
    };

    if (!bytes) {
      onError(Ci.nsISearchService.ERROR_DOWNLOAD_FAILURE);
      return;
    }

    var parser = new DOMParser();
    var doc = parser.parseFromBuffer(bytes, "text/xml");
    this._data = doc.documentElement;

    try {
      this._initFromData();
    } catch (ex) {
      lazy.logConsole.error("_onLoad: Failed to init engine!", ex);

      if (ex.result == Cr.NS_ERROR_FILE_CORRUPTED) {
        onError(Ci.nsISearchService.ERROR_ENGINE_CORRUPTED);
      } else {
        onError(Ci.nsISearchService.ERROR_DOWNLOAD_FAILURE);
      }
      return;
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
      // Check that when adding a new engine (e.g., not updating an
      // existing one), a duplicate engine does not already exist.
      if (Services.search.getEngineByName(this.name)) {
        onError(Ci.nsISearchService.ERROR_DUPLICATE_ENGINE);
        lazy.logConsole.debug("_onLoad: duplicate engine found, bailing");
        return;
      }

      this._loadPath = OpenSearchEngine.getAnonymizedLoadPath(
        lazy.SearchUtils.sanitizeName(this.name),
        this._uri
      );
      if (this._extensionID) {
        this._loadPath += ":" + this._extensionID;
      }
      this.setAttr(
        "loadPathHash",
        lazy.SearchUtils.getVerificationHash(this._loadPath)
      );
    }

    // Notify the search service of the successful load. It will deal with
    // updates by checking this._engineToUpdate.
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.LOADED);

    callback?.();
  }

  /**
   * Initialize this Engine object from the collected data.
   */
  _initFromData() {
    ENSURE_WARN(
      this._data,
      "Can't init an engine with no data!",
      Cr.NS_ERROR_UNEXPECTED
    );

    // Ensure we have a supported engine type before attempting to parse it.
    let element = this._data;
    if (
      (element.localName == MOZSEARCH_LOCALNAME &&
        element.namespaceURI == MOZSEARCH_NS_10) ||
      (element.localName == OPENSEARCH_LOCALNAME &&
        OPENSEARCH_NAMESPACES.includes(element.namespaceURI))
    ) {
      lazy.logConsole.debug("Initing search plugin from", this._location);

      this._parse();
    } else {
      Cu.reportError("Invalid search plugin due to namespace not matching.");
      throw Components.Exception(
        this._location + " is not a valid search plugin.",
        Cr.NS_ERROR_FILE_CORRUPTED
      );
    }
    // No need to keep a ref to our data (which in some cases can be a document
    // element) past this point
    this._data = null;
  }

  /**
   * Extracts data from an OpenSearch URL element and creates an EngineURL
   * object which is then added to the engine's list of URLs.
   *
   * @param {HTMLLinkElement} element
   *   The OpenSearch URL element.
   * @throws NS_ERROR_FAILURE if a URL object could not be created.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag.
   * @see EngineURL()
   */
  _parseURL(element) {
    var type = element.getAttribute("type");
    // According to the spec, method is optional, defaulting to "GET" if not
    // specified
    var method = element.getAttribute("method") || "GET";
    var template = element.getAttribute("template");

    let rels = [];
    if (element.hasAttribute("rel")) {
      rels = element
        .getAttribute("rel")
        .toLowerCase()
        .split(/\s+/);
    }

    // Support an alternate suggestion type, see bug 1425827 for details.
    if (type == "application/json" && rels.includes("suggestions")) {
      type = lazy.SearchUtils.URL_TYPE.SUGGEST_JSON;
    }

    try {
      var url = new EngineURL(type, method, template);
    } catch (ex) {
      throw Components.Exception(
        "_parseURL: failed to add " + template + " as a URL",
        Cr.NS_ERROR_FAILURE
      );
    }

    if (rels.length) {
      url.rels = rels;
    }

    for (var i = 0; i < element.children.length; ++i) {
      var param = element.children[i];
      if (param.localName == "Param") {
        try {
          url.addParam(param.getAttribute("name"), param.getAttribute("value"));
        } catch (ex) {
          // Ignore failure
          lazy.logConsole.error("_parseURL: Url element has an invalid param");
        }
      }
      // Note: MozParams are not supported for OpenSearch engines as they
      // cannot be app-provided engines.
    }

    this._urls.push(url);
  }

  /**
   * Get the icon from an OpenSearch Image element.
   *
   * @param {HTMLLinkElement} element
   *   The OpenSearch URL element.
   * @see http://opensearch.a9.com/spec/1.1/description/#image
   */
  _parseImage(element) {
    let width = parseInt(element.getAttribute("width"), 10);
    let height = parseInt(element.getAttribute("height"), 10);
    let isPrefered = width == 16 && height == 16;

    if (isNaN(width) || isNaN(height) || width <= 0 || height <= 0) {
      lazy.logConsole.warn(
        "OpenSearch image element must have positive width and height."
      );
      return;
    }

    this._setIcon(element.textContent, isPrefered, width, height);
  }

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parse() {
    var doc = this._data;

    for (var i = 0; i < doc.children.length; ++i) {
      var child = doc.children[i];
      switch (child.localName) {
        case "ShortName":
          this._name = child.textContent;
          break;
        case "Description":
          this._description = child.textContent;
          break;
        case "Url":
          try {
            this._parseURL(child);
          } catch (ex) {
            // Parsing of the element failed, just skip it.
            lazy.logConsole.error("Failed to parse URL child:", ex);
          }
          break;
        case "Image":
          this._parseImage(child);
          break;
        case "InputEncoding":
          // If this is not specified we fallback to the SearchEngine constructor
          // which currently uses SearchUtils.DEFAULT_QUERY_CHARSET which is
          // UTF-8 - the same as for OpenSearch.
          this._queryCharset = child.textContent;
          break;

        // Non-OpenSearch elements
        case "SearchForm":
          this._searchForm = child.textContent;
          break;
        case "UpdateUrl":
          this._updateURL = child.textContent;
          break;
        case "UpdateInterval":
          this._updateInterval = parseInt(child.textContent);
          break;
        case "IconUpdateUrl":
          this._iconUpdateURL = child.textContent;
          break;
        case "ExtensionID":
          this._extensionID = child.textContent;
          break;
      }
    }
    if (!this.name || !this._urls.length) {
      throw Components.Exception(
        "_parse: No name, or missing URL!",
        Cr.NS_ERROR_FAILURE
      );
    }
    if (!this.supportsResponseType(lazy.SearchUtils.URL_TYPE.SEARCH)) {
      throw Components.Exception(
        "_parse: No text/html result type!",
        Cr.NS_ERROR_FAILURE
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

  // This indicates where we found the .xml file to load the engine,
  // and attempts to hide user-identifiable data (such as username).
  static getAnonymizedLoadPath(shortName, uri) {
    return `[${uri.scheme}]${uri.host}/${shortName}.xml`;
  }
}
