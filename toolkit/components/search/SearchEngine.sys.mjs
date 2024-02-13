/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchEngine",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

const USER_DEFINED = "searchTerms";

// Supported OpenSearch parameters
// See http://opensearch.a9.com/spec/1.1/querysyntax/#core
const OS_PARAM_INPUT_ENCODING = "inputEncoding";
const OS_PARAM_LANGUAGE = "language";
const OS_PARAM_OUTPUT_ENCODING = "outputEncoding";

// Default values
const OS_PARAM_LANGUAGE_DEF = "*";
const OS_PARAM_OUTPUT_ENCODING_DEF = "UTF-8";

// "Unsupported" OpenSearch parameters. For example, we don't support
// page-based results, so if the engine requires that we send the "page index"
// parameter, we'll always send "1".
const OS_PARAM_COUNT = "count";
const OS_PARAM_START_INDEX = "startIndex";
const OS_PARAM_START_PAGE = "startPage";

// Default values
const OS_PARAM_COUNT_DEF = "20"; // 20 results
const OS_PARAM_START_INDEX_DEF = "1"; // start at 1st result
const OS_PARAM_START_PAGE_DEF = "1"; // 1st page

// A array of arrays containing parameters that we don't fully support, and
// their default values. We will only send values for these parameters if
// required, since our values are just really arbitrary "guesses" that should
// give us the output we want.
var OS_UNSUPPORTED_PARAMS = [
  [OS_PARAM_COUNT, OS_PARAM_COUNT_DEF],
  [OS_PARAM_START_INDEX, OS_PARAM_START_INDEX_DEF],
  [OS_PARAM_START_PAGE, OS_PARAM_START_PAGE_DEF],
];

/**
 * Truncates big blobs of (data-)URIs to console-friendly sizes
 *
 * @param {string} str
 *   String to tone down
 * @param {number} len
 *   Maximum length of the string to return. Defaults to the length of a tweet.
 * @returns {string}
 *   The shortend string.
 */
function limitURILength(str, len) {
  len = len || 140;
  if (str.length > len) {
    return str.slice(0, len) + "...";
  }
  return str;
}

/**
 * Tries to rescale an icon to a given size.
 *
 * @param {Array} byteArray
 *   Byte array containing the icon payload.
 * @param {string} contentType
 *   Mime type of the payload.
 * @param {number} [size]
 *   Desired icon size.
 * @returns {Array}
 *   An array of two elements - an array of integers and a string for the content
 *   type.
 * @throws if the icon cannot be rescaled or the rescaled icon is too big.
 */
function rescaleIcon(byteArray, contentType, size = 32) {
  if (contentType == "image/svg+xml") {
    throw new Error("Cannot rescale SVG image");
  }

  let imgTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
  let arrayBuffer = new Int8Array(byteArray).buffer;
  let container = imgTools.decodeImageFromArrayBuffer(arrayBuffer, contentType);
  let stream = imgTools.encodeScaledImage(container, "image/png", size, size);
  let streamSize = stream.available();
  if (streamSize > lazy.SearchUtils.MAX_ICON_SIZE) {
    throw new Error("Icon is too big");
  }
  let bis = new BinaryInputStream(stream);
  return [bis.readByteArray(streamSize), "image/png"];
}

/**
 * A simple class to handle caching of preferences that may be read from
 * parameters.
 */
const ParamPreferenceCache = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  initCache() {
    // Preference params are normally only on the default branch to avoid these being easily changed.
    // We allow them on the normal branch in nightly builds to make testing easier.
    let branchFetcher = AppConstants.NIGHTLY_BUILD
      ? "getBranch"
      : "getDefaultBranch";
    this.branch = Services.prefs[branchFetcher](
      lazy.SearchUtils.BROWSER_SEARCH_PREF + "param."
    );
    this.cache = new Map();
    this.nimbusCache = new Map();
    for (let prefName of this.branch.getChildList("")) {
      this.cache.set(prefName, this.branch.getCharPref(prefName, null));
    }
    this.branch.addObserver("", this, true);

    this.onNimbusUpdate = this.onNimbusUpdate.bind(this);
    this.onNimbusUpdate();
    lazy.NimbusFeatures.search.onUpdate(this.onNimbusUpdate);
    lazy.NimbusFeatures.search.ready().then(this.onNimbusUpdate);
  },

  observe(subject, topic, data) {
    this.cache.set(data, this.branch.getCharPref(data, null));
  },

  onNimbusUpdate() {
    let extraParams =
      lazy.NimbusFeatures.search.getVariable("extraParams") || [];
    this.nimbusCache.clear();
    // The try catch ensures that if the params were incorrect for some reason,
    // the search service can still startup properly.
    try {
      for (const { key, value } of extraParams) {
        this.nimbusCache.set(key, value);
      }
    } catch (ex) {
      console.error("Failed to load nimbus variables for extraParams:", ex);
    }
  },

  getPref(prefName) {
    if (!this.cache) {
      this.initCache();
    }
    return this.nimbusCache.has(prefName)
      ? this.nimbusCache.get(prefName)
      : this.cache.get(prefName);
  },
};

/**
 * Represents a name/value pair for a parameter
 */
class QueryParameter {
  /**
   * @param {string} name
   *   The parameter's name. Must not be null.
   * @param {string} value
   *   The value of the parameter. May be an empty string, must not be null or
   *   undefined.
   * @param {string} purpose
   *   The search purpose for which matches when this parameter should be
   *   applied, e.g. "searchbar", "contextmenu".
   */
  constructor(name, value, purpose = null) {
    if (!name || value == null) {
      throw Components.Exception(
        "missing name or value for QueryParameter!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.name = name;
    this._value = value;
    this.purpose = purpose;
  }

  get value() {
    return this._value;
  }

  toJSON() {
    const result = {
      name: this.name,
      value: this.value,
    };
    if (this.purpose) {
      result.purpose = this.purpose;
    }
    return result;
  }
}

/**
 * Represents a special paramater that can be set by preferences. The
 * value is read from the 'browser.search.param.*' default preference
 * branch.
 */
class QueryPreferenceParameter extends QueryParameter {
  /**
   * @param {string} name
   *   The name of the parameter as injected into the query string.
   * @param {string} prefName
   *   The name of the preference to read from the branch.
   * @param {string} purpose
   *   The search purpose for which matches when this parameter should be
   *   applied, e.g. `searchbar`, `contextmenu`.
   */
  constructor(name, prefName, purpose) {
    super(name, prefName, purpose);
  }

  get value() {
    const prefValue = ParamPreferenceCache.getPref(this._value);
    return prefValue ? encodeURIComponent(prefValue) : null;
  }

  /**
   * Converts the object to json. This object is converted with a mozparam flag
   * as it gets written to the cache and hence we then know what type it is
   * when reading it back.
   *
   * @returns {object}
   */
  toJSON() {
    const result = {
      condition: "pref",
      mozparam: true,
      name: this.name,
      pref: this._value,
    };
    if (this.purpose) {
      result.purpose = this.purpose;
    }
    return result;
  }
}

/**
 * Perform OpenSearch parameter substitution on aParamValue.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#core
 *
 * @param {string} paramValue
 *   The OpenSearch search parameters.
 * @param {string} searchTerms
 *   The user-provided search terms. This string will inserted into
 *   paramValue as the value of the OS_PARAM_USER_DEFINED parameter.
 *   This value must already be escaped appropriately - it is inserted
 *   as-is.
 * @param {nsISearchEngine} engine
 *   The engine which owns the string being acted on.
 * @returns {string}
 *   An updated parameter string.
 */
function ParamSubstitution(paramValue, searchTerms, engine) {
  const PARAM_REGEXP = /\{((?:\w+:)?\w+)(\??)\}/g;
  return paramValue.replace(PARAM_REGEXP, function (match, name, optional) {
    // {searchTerms} is by far the most common param so handle it first.
    if (name == USER_DEFINED) {
      return searchTerms;
    }

    // {inputEncoding} is the second most common param.
    if (name == OS_PARAM_INPUT_ENCODING) {
      return engine.queryCharset;
    }

    // moz: parameters are only available for default search engines.
    if (engine.isAppProvided && name == lazy.SearchUtils.MOZ_PARAM.LOCALE) {
      return Services.locale.requestedLocale;
    }

    // Handle the less common OpenSearch parameters we're confident about.
    if (name == OS_PARAM_LANGUAGE) {
      return Services.locale.requestedLocale || OS_PARAM_LANGUAGE_DEF;
    }
    if (name == OS_PARAM_OUTPUT_ENCODING) {
      return OS_PARAM_OUTPUT_ENCODING_DEF;
    }

    // At this point, if a parameter is optional, just omit it.
    if (optional) {
      return "";
    }

    // Replace unsupported parameters that only have hardcoded default values.
    for (let param of OS_UNSUPPORTED_PARAMS) {
      if (name == param[0]) {
        return param[1];
      }
    }

    // Don't replace unknown non-optional parameters.
    return match;
  });
}

/**
 * EngineURL holds a query URL and all associated parameters.
 */
export class EngineURL {
  params = [];
  rels = [];

  /**
   * Creates an EngineURL.
   *
   * @param {string} mimeType
   *   The name of the MIME type of the search results returned by this URL.
   * @param {string} requestMethod
   *   The HTTP request method. Must be a case insensitive value of either
   *   "GET" or "POST".
   * @param {string} template
   *   The URL to which search queries should be sent. For GET requests,
   *   must contain the string "{searchTerms}", to indicate where the user
   *   entered search terms should be inserted.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
   *
   * @throws NS_ERROR_NOT_IMPLEMENTED if aType is unsupported.
   */
  constructor(mimeType, requestMethod, template) {
    if (!mimeType || !requestMethod || !template) {
      throw Components.Exception(
        "missing mimeType, method or template for EngineURL!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    var method = requestMethod.toUpperCase();
    var type = mimeType.toLowerCase();

    if (method != "GET" && method != "POST") {
      throw Components.Exception(
        'method passed to EngineURL must be "GET" or "POST"',
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.type = type;
    this.method = method;

    var templateURI = lazy.SearchUtils.makeURI(template);
    if (!templateURI) {
      throw Components.Exception(
        "new EngineURL: template is not a valid URI!",
        Cr.NS_ERROR_FAILURE
      );
    }

    switch (templateURI.scheme) {
      case "http":
      case "https":
        this.template = template;
        break;
      default:
        throw Components.Exception(
          "new EngineURL: template uses invalid scheme!",
          Cr.NS_ERROR_FAILURE
        );
    }

    this.templateHost = templateURI.host;
  }

  addParam(name, value, purpose) {
    this.params.push(new QueryParameter(name, value, purpose));
  }

  /**
   * Adds a MozParam to the parameters list for this URL. For purpose based params
   * these are saved as standard parameters, for preference based we save them
   * as a special type.
   *
   * @param {object} param
   *   The parameter to add.
   * @param {string} param.name
   *   The name of the parameter to add to the url.
   * @param {string} [param.condition]
   *   The type of parameter this is, e.g. "pref" for a preference parameter,
   *   or "purpose" for a value-based parameter with a specific purpose. The
   *   default is "purpose".
   * @param {string} [param.value]
   *   The value if it is a "purpose" parameter.
   * @param {string} [param.purpose]
   *   The purpose of the parameter for when it is applied, e.g. for `searchbar`
   *   searches.
   * @param {string} [param.pref]
   *   The preference name of the parameter, that gets appended to
   *   `browser.search.param.`.
   */
  _addMozParam(param) {
    const purpose = param.purpose || undefined;
    if (param.condition && param.condition == "pref") {
      this.params.push(
        new QueryPreferenceParameter(param.name, param.pref, purpose)
      );
    } else {
      this.addParam(param.name, param.value || undefined, purpose);
    }
  }

  getSubmission(searchTerms, engine, purpose) {
    var url = ParamSubstitution(this.template, searchTerms, engine);
    // Default to searchbar if the purpose is not provided
    var requestPurpose = purpose || "searchbar";

    // If a particular purpose isn't defined in the plugin, fallback to 'searchbar'.
    if (
      requestPurpose != "searchbar" &&
      !this.params.some(p => p.purpose && p.purpose == requestPurpose)
    ) {
      requestPurpose = "searchbar";
    }

    // Create an application/x-www-form-urlencoded representation of our params
    // (name=value&name=value&name=value)
    let dataArray = [];
    for (var i = 0; i < this.params.length; ++i) {
      var param = this.params[i];

      // If this parameter has a purpose, only add it if the purpose matches
      if (param.purpose && param.purpose != requestPurpose) {
        continue;
      }

      // Preference MozParams might not have a preferenced saved, or a valid value.
      if (param.value != null) {
        var value = ParamSubstitution(param.value, searchTerms, engine);

        dataArray.push(param.name + "=" + value);
      }
    }
    let dataString = dataArray.join("&");

    var postData = null;
    if (this.method == "GET") {
      // GET method requests have no post data, and append the encoded
      // query string to the url...
      if (dataString) {
        if (url.includes("?")) {
          url = `${url}&${dataString}`;
        } else {
          url = `${url}?${dataString}`;
        }
      }
    } else if (this.method == "POST") {
      // POST method requests must wrap the encoded text in a MIME
      // stream and supply that as POSTDATA.
      var stringStream = Cc[
        "@mozilla.org/io/string-input-stream;1"
      ].createInstance(Ci.nsIStringInputStream);
      stringStream.data = dataString;

      postData = Cc["@mozilla.org/network/mime-input-stream;1"].createInstance(
        Ci.nsIMIMEInputStream
      );
      postData.addHeader("Content-Type", "application/x-www-form-urlencoded");
      postData.setData(stringStream);
    }

    return new Submission(Services.io.newURI(url), postData);
  }

  _getTermsParameterName() {
    let searchTerms = "{" + USER_DEFINED + "}";
    let paramName = this.params.find(p => p.value == searchTerms)?.name;
    // Some query params might not be added to this.params
    // in the engine construction process, so try checking the URL
    // template for the existence of the query parameter value.
    if (!paramName) {
      let urlParms = new URL(this.template).searchParams;
      for (let [name, value] of urlParms.entries()) {
        if (value == searchTerms) {
          paramName = name;
          break;
        }
      }
    }
    return paramName ?? "";
  }

  _hasRelation(rel) {
    return this.rels.some(e => e == rel.toLowerCase());
  }

  _initWithJSON(json) {
    if (!json.params) {
      return;
    }

    this.rels = json.rels;

    for (let i = 0; i < json.params.length; ++i) {
      let param = json.params[i];
      // mozparam and purpose are only supported for app-provided engines.
      // Since we do not store the details for those engines, we don't want
      // to handle it here.
      if (!param.mozparam && !param.purpose) {
        this.addParam(param.name, param.value);
      }
    }
  }

  /**
   * Creates a JavaScript object that represents this URL.
   *
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    var json = {
      params: this.params,
      rels: this.rels,
      template: this.template,
    };

    if (this.type != lazy.SearchUtils.URL_TYPE.SEARCH) {
      json.type = this.type;
    }
    if (this.method != "GET") {
      json.method = this.method;
    }

    return json;
  }
}

/**
 * SearchEngine represents WebExtension based search engines.
 */
export class SearchEngine {
  QueryInterface = ChromeUtils.generateQI(["nsISearchEngine"]);
  // Data set by the user.
  _metaData = {};
  // Anonymized path of where we initially loaded the engine from.
  // This will stay null for engines installed in the profile before we moved
  // to a JSON storage.
  _loadPath = null;
  // The engine's description
  _description = "";
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon = null;
  // The engine's name.
  _name = null;
  // The name of the charset used to submit the search terms.
  _queryCharset = null;
  // The engine's raw SearchForm value (URL string pointing to a search form).
  #cachedSearchForm = null;
  // The order hint from the configuration (if any).
  _orderHint = null;
  // The telemetry id from the configuration (if any).
  _telemetryId = null;
  // Set to true once the engine has been added to the store, and the initial
  // notification sent. This allows to skip sending notifications during
  // initialization.
  _engineAddedToStore = false;
  // The aliases coming from the engine definition (via webextension
  // keyword field for example).
  _definedAliases = [];
  // The urls associated with this engine.
  _urls = [];
  // The query parameter name of the search url, cached in memory to avoid
  // repeated look-ups.
  _searchUrlQueryParamName = null;
  // The known public suffix of the search url, cached in memory to avoid
  // repeated look-ups.
  _searchUrlPublicSuffix = null;
  /**
   * The unique id of the Search Engine.
   * The id is an UUID.
   *
   * @type {string}
   */
  #id;

  /**
   *  Creates a Search Engine.
   *
   * @param {object} options
   *   The options for this search engine.
   * @param {string} [options.id]
   *   The identifier to use for this engine, if none is specified a random
   *   uuid is created.
   * @param {string} options.loadPath
   *   The path of the engine was originally loaded from. Should be anonymized.
   */
  constructor(options = {}) {
    this.#id = options.id ?? this.#uuid();
    if (!("loadPath" in options)) {
      throw new Error("loadPath missing from options.");
    }
    this._loadPath = options.loadPath;
  }

  get _searchForm() {
    return this.#cachedSearchForm;
  }
  set _searchForm(value) {
    if (/^https?:/i.test(value)) {
      this.#cachedSearchForm = value;
    } else {
      lazy.logConsole.debug(
        "_searchForm: Invalid URL dropped for",
        this._name || "the current engine"
      );
    }
  }

  /**
   * Attempts to find an EngineURL object in the set of EngineURLs for
   * this Engine that has the given type string.  (This corresponds to the
   * "type" attribute in the "Url" node in the OpenSearch spec.)
   *
   * @param {string} type
   *   The type to match the EngineURL's type attribute.
   * @param {string} [rel]
   *   Only return URLs that with this rel value.
   * @returns {EngineURL|null}
   *   Returns the first matching URL found, null otherwise.
   */
  _getURLOfType(type, rel) {
    for (let url of this._urls) {
      if (url.type == type && (!rel || url._hasRelation(rel))) {
        return url;
      }
    }

    return null;
  }

  /**
   * Creates a key by serializing an object that contains the icon's width
   * and height.
   *
   * @param {number} width
   *   Width of the icon.
   * @param {number} height
   *   Height of the icon.
   * @returns {string}
   *   Key string.
   */
  _getIconKey(width, height) {
    let keyObj = {
      width,
      height,
    };

    return JSON.stringify(keyObj);
  }

  /**
   * Add an icon to the icon map used by getIconURL().
   *
   * @param {number} width
   *   Width of the icon.
   * @param {number} height
   *   Height of the icon.
   * @param {string} uriSpec
   *   String with the icon's URI.
   */
  _addIconToMap(width, height, uriSpec) {
    if (width == 16 && height == 16) {
      // The 16x16 icon is stored in _iconURL, we don't need to store it twice.
      return;
    }

    // Use an object instead of a Map() because it needs to be serializable.
    this._iconMapObj = this._iconMapObj || {};
    let key = this._getIconKey(width, height);
    this._iconMapObj[key] = uriSpec;
  }

  /**
   * Sets the .iconURI property of the engine. If both aWidth and aHeight are
   * provided an entry will be added to _iconMapObj that will enable accessing
   * icon's data through getIconURL() APIs.
   *
   * @param {string} iconURL
   *   A URI string pointing to the engine's icon. Must have a http[s]
   *   or data scheme. Icons with HTTP[S] schemes will be
   *   downloaded and converted to data URIs for storage in the engine
   *   XML files, if the engine is not built-in.
   * @param {boolean} isPreferred
   *   Whether or not this icon is to be preferred. Preferred icons can
   *   override non-preferred icons.
   * @param {number} [width]
   *   Width of the icon.
   * @param {number} [height]
   *   Height of the icon.
   */
  _setIcon(iconURL, isPreferred, width, height) {
    var uri = lazy.SearchUtils.makeURI(iconURL);

    // Ignore bad URIs
    if (!uri) {
      return;
    }

    lazy.logConsole.debug(
      "_setIcon: Setting icon url for",
      this.name,
      "to",
      limitURILength(uri.spec)
    );
    // Only accept remote icons from http[s]
    switch (uri.scheme) {
      // Fall through to the data case
      case "moz-extension":
      case "data":
        if (!this._hasPreferredIcon || isPreferred) {
          this._iconURI = uri;

          this._hasPreferredIcon = isPreferred;
        }

        if (width && height) {
          this._addIconToMap(width, height, iconURL);
        }
        break;
      case "http":
      case "https":
        let iconLoadCallback = function (byteArray, contentType) {
          // This callback may run after we've already set a preferred icon,
          // so check again.
          if (this._hasPreferredIcon && !isPreferred) {
            return;
          }

          if (!byteArray) {
            lazy.logConsole.warn("iconLoadCallback: load failed");
            return;
          }

          if (byteArray.length > lazy.SearchUtils.MAX_ICON_SIZE) {
            try {
              lazy.logConsole.debug("iconLoadCallback: rescaling icon");
              [byteArray, contentType] = rescaleIcon(byteArray, contentType);
            } catch (ex) {
              lazy.logConsole.error(
                "Unable to set icon for the search engine:",
                ex
              );
              return;
            }
          }

          let dataURL =
            "data:" +
            contentType +
            ";base64," +
            btoa(String.fromCharCode.apply(null, byteArray));

          this._iconURI = lazy.SearchUtils.makeURI(dataURL);

          if (width && height) {
            this._addIconToMap(width, height, dataURL);
          }

          if (this._engineAddedToStore) {
            lazy.SearchUtils.notifyAction(
              this,
              lazy.SearchUtils.MODIFIED_TYPE.CHANGED
            );
          }
          this._hasPreferredIcon = isPreferred;
        };

        let chan = lazy.SearchUtils.makeChannel(
          uri,
          Ci.nsIContentPolicy.TYPE_IMAGE
        );
        let listener = new lazy.SearchUtils.LoadListener(
          chan,
          /^image\//,
          iconLoadCallback.bind(this)
        );
        chan.notificationCallbacks = listener;
        chan.asyncOpen(listener);
        break;
    }
  }

  /**
   * Initialize an EngineURL object from metadata.
   *
   * @param {string} type
   *   The url type.
   * @param {object} params
   *   The URL parameters.
   * @param {string | Array} [params.getParams]
   *   Any parameters for a GET method. This is either a query string, or
   *   an array of objects which have name/value pairs.
   * @param {string} [params.method]
   *   The type of method, defaults to GET.
   * @param {string} [params.mozParams]
   *   Any special Mozilla Parameters.
   * @param {string | Array} [params.postParams]
   *   Any parameters for a POST method. This is either a query string, or
   *   an array of objects which have name/value pairs.
   * @param {string} params.template
   *   The url template.
   * @returns {EngineURL}
   *   The newly created EngineURL.
   */
  _getEngineURLFromMetaData(type, params) {
    let url = new EngineURL(type, params.method || "GET", params.template);

    // Do the MozParams first, so that we are more likely to get the query
    // on the end of the URL, rather than the MozParams (xref bug 1484232).
    if (params.mozParams) {
      for (let p of params.mozParams) {
        if ((p.condition || p.purpose) && !this.isAppProvided) {
          continue;
        }
        url._addMozParam(p);
      }
    }
    if (params.postParams) {
      if (Array.isArray(params.postParams)) {
        for (let { name, value } of params.postParams) {
          url.addParam(name, value);
        }
      } else {
        for (let [name, value] of new URLSearchParams(params.postParams)) {
          url.addParam(name, value);
        }
      }
    }

    if (params.getParams) {
      if (Array.isArray(params.getParams)) {
        for (let { name, value } of params.getParams) {
          url.addParam(name, value);
        }
      } else {
        for (let [name, value] of new URLSearchParams(params.getParams)) {
          url.addParam(name, value);
        }
      }
    }

    return url;
  }

  /**
   * Initialize this engine object.
   *
   * @param {object} details
   *   The details of the engine.
   * @param {string} details.name
   *   The name of the engine.
   * @param {string} details.keyword
   *   The keyword for the engine.
   * @param {string} details.iconURL
   *   The url to use for the icon of the engine.
   * @param {string} details.search_url
   *   The search url template for the engine.
   * @param {string} [details.search_url_get_params]
   *   The search url parameters for use with the GET method.
   * @param {string} [details.search_url_post_params]
   *   The search url parameters for use with the POST method.
   * @param {object} [details.params]
   *   Any special Mozilla parameters.
   * @param {string} [details.suggest_url]
   *   The suggestion url template for the engine.
   * @param {string} [details.suggest_url_get_params]
   *   The suggestion url parameters for use with the GET method.
   * @param {string} [details.suggest_url_post_params]
   *   The suggestion url parameters for use with the POST method.
   * @param {string} [details.encoding]
   *   The encoding to use for the engine.
   * @param {string} [details.search_form]
   *   THe search form url for the engine.
   * @param {object} [configuration]
   *   The search engine configuration for application provided engines, that
   *   may be overriding some of the WebExtension's settings.
   */
  _initWithDetails(details, configuration = {}) {
    this._orderHint = configuration.orderHint;
    this._name = details.name.trim();

    this._definedAliases = [];
    if (Array.isArray(details.keyword)) {
      this._definedAliases = details.keyword.map(k => k.trim());
    } else if (details.keyword?.trim()) {
      this._definedAliases = [details.keyword?.trim()];
    }

    this._description = details.description;
    if (details.iconURL) {
      this._setIcon(details.iconURL, true);
    }
    this._setUrls(details, configuration);
  }

  /**
   * This sets the urls for the search engine based on the supplied parameters.
   * If you add anything here, please consider if it needs to be handled in the
   * overrideWithExtension / removeExtensionOverride functions as well.
   *
   * @param {object} details
   *   The details of the engine.
   * @param {string} details.search_url
   *   The search url template for the engine.
   * @param {string} [details.search_url_get_params]
   *   The search url parameters for use with the GET method.
   * @param {string} [details.search_url_post_params]
   *   The search url parameters for use with the POST method.
   * @param {object} [details.params]
   *   Any special Mozilla parameters.
   * @param {string} [details.suggest_url]
   *   The suggestion url template for the engine.
   * @param {string} [details.suggest_url_get_params]
   *   The suggestion url parameters for use with the GET method.
   * @param {string} [details.suggest_url_post_params]
   *   The suggestion url parameters for use with the POST method.
   * @param {string} [details.encoding]
   *   The encoding to use for the engine.
   * @param {string} [details.search_form]
   *   THe search form url for the engine.
   * @param {object} [configuration]
   *   The search engine configuration for application provided engines, that
   *   may be overriding some of the WebExtension's settings.
   */
  _setUrls(details, configuration = {}) {
    let postParams =
      configuration.params?.searchUrlPostParams ||
      details.search_url_post_params ||
      "";
    let url = this._getEngineURLFromMetaData(lazy.SearchUtils.URL_TYPE.SEARCH, {
      method: (postParams && "POST") || "GET",
      // AddonManager will sometimes encode the URL via `new URL()`. We want
      // to ensure we're always dealing with decoded urls.
      template: decodeURI(details.search_url),
      getParams:
        configuration.params?.searchUrlGetParams ||
        details.search_url_get_params ||
        "",
      postParams,
      mozParams: configuration.extraParams || details.params || [],
    });

    this._urls.push(url);

    if (details.suggest_url) {
      let suggestPostParams =
        configuration.params?.suggestUrlPostParams ||
        details.suggest_url_post_params ||
        "";
      url = this._getEngineURLFromMetaData(
        lazy.SearchUtils.URL_TYPE.SUGGEST_JSON,
        {
          method: (suggestPostParams && "POST") || "GET",
          // suggest_url doesn't currently get encoded.
          template: details.suggest_url,
          getParams:
            configuration.params?.suggestUrlGetParams ||
            details.suggest_url_get_params ||
            "",
          postParams: suggestPostParams,
          mozParams: configuration.suggestExtraParams || [],
        }
      );

      this._urls.push(url);
    }

    if (configuration?.urls?.trending) {
      let trending = this._getEngineURLFromMetaData(
        lazy.SearchUtils.URL_TYPE.TRENDING_JSON,
        {
          method: "GET",
          template: decodeURI(configuration.urls.trending.fullPath),
          getParams: configuration.urls.trending.query,
        }
      );
      this._urls.push(trending);
    }

    if (configuration.clickUrl) {
      this.clickUrl = configuration.clickUrl;
    }

    if (details.encoding) {
      this._queryCharset = details.encoding;
    }
    this.#cachedSearchForm = details.search_form;
  }

  checkSearchUrlMatchesManifest(details) {
    let existingUrl = this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH);

    let newUrl = this._getEngineURLFromMetaData(
      lazy.SearchUtils.URL_TYPE.SEARCH,
      {
        method: (details.search_url_post_params && "POST") || "GET",
        // AddonManager will sometimes encode the URL via `new URL()`. We want
        // to ensure we're always dealing with decoded urls.
        template: decodeURI(details.search_url),
        getParams: details.search_url_get_params || "",
        postParams: details.search_url_post_params || "",
      }
    );

    let existingSubmission = existingUrl.getSubmission("", this);
    let newSubmission = newUrl.getSubmission("", this);

    return (
      existingSubmission.uri.equals(newSubmission.uri) &&
      existingSubmission.postData?.data.data ==
        newSubmission.postData?.data.data
    );
  }

  /**
   * Overrides the urls/parameters with those of the provided extension.
   * The parameters are not saved to the search settings - the code handling
   * the extension should set these on every restart, this avoids potential
   * third party modifications and means that we can verify the WebExtension is
   * still in the allow list.
   *
   * @param {string} extensionID
   *   The WebExtension ID. For Policy engines, this is currently "set-via-policy".
   * @param {object} manifest
   *   An object representing the WebExtensions' manifest.
   */
  overrideWithExtension(extensionID, manifest) {
    this._overriddenData = {
      urls: this._urls,
      queryCharset: this._queryCharset,
      searchForm: this.#cachedSearchForm,
    };
    this._urls = [];
    this.setAttr("overriddenBy", extensionID);
    this._setUrls(manifest.chrome_settings_overrides.search_provider);
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Resets the overrides for the engine if it has been overridden.
   */
  removeExtensionOverride() {
    if (this.getAttr("overriddenBy")) {
      // If the attribute is set, but there is no data, skip it. Worst case,
      // the urls will be reset on a restart.
      if (this._overriddenData) {
        this._urls = this._overriddenData.urls;
        this._queryCharset = this._overriddenData.queryCharset;
        this.#cachedSearchForm = this._overriddenData.searchForm;
        delete this._overriddenData;
      } else {
        lazy.logConsole.error(
          `${this._name} had overriddenBy set, but no _overriddenData`
        );
      }
      this.clearAttr("overriddenBy");
      lazy.SearchUtils.notifyAction(
        this,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );
    }
  }

  /**
   * Init from a JSON record.
   *
   * @param {object} json
   *   The json record to use.
   */
  _initWithJSON(json) {
    this.#id = json.id ?? this.#id;
    this._name = json._name;
    this._description = json.description;
    this._hasPreferredIcon = json._hasPreferredIcon == undefined;
    this._queryCharset =
      json.queryCharset || lazy.SearchUtils.DEFAULT_QUERY_CHARSET;
    this.#cachedSearchForm = json.__searchForm;
    this._iconURI = lazy.SearchUtils.makeURI(json._iconURL);
    this._iconMapObj = json._iconMapObj || null;
    this._metaData = json._metaData || {};
    this._orderHint = json._orderHint || null;
    this._definedAliases = json._definedAliases || [];
    // These changed keys in Firefox 80, maintain the old keys
    // for backwards compatibility.
    if (json._definedAlias) {
      this._definedAliases.push(json._definedAlias);
    }
    this._filePath = json.filePath || json._filePath || null;

    for (let i = 0; i < json._urls.length; ++i) {
      let url = json._urls[i];
      let engineURL = new EngineURL(
        url.type || lazy.SearchUtils.URL_TYPE.SEARCH,
        url.method || "GET",
        url.template
      );
      engineURL._initWithJSON(url);
      this._urls.push(engineURL);
    }
  }

  /**
   * Creates a JavaScript object that represents this engine.
   *
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    const fieldsToCopy = [
      "id",
      "_name",
      "_loadPath",
      "description",
      "_iconURL",
      "_iconMapObj",
      "_metaData",
      "_urls",
      "_orderHint",
      "_telemetryId",
      "_filePath",
      "_definedAliases",
    ];

    let json = {};
    for (const field of fieldsToCopy) {
      if (field in this) {
        json[field] = this[field];
      }
    }

    if (this.#cachedSearchForm) {
      json.__searchForm = this.#cachedSearchForm;
    }
    if (!this._hasPreferredIcon) {
      json._hasPreferredIcon = this._hasPreferredIcon;
    }
    if (this.queryCharset != lazy.SearchUtils.DEFAULT_QUERY_CHARSET) {
      json.queryCharset = this.queryCharset;
    }

    return json;
  }

  setAttr(name, val) {
    this._metaData[name] = val;
  }

  getAttr(name) {
    return this._metaData[name] || undefined;
  }

  clearAttr(name) {
    delete this._metaData[name];
  }

  /**
   * Get the user-defined alias.
   *
   * @type {string}
   */
  get alias() {
    return this.getAttr("alias") || "";
  }

  set alias(val) {
    var value = val ? val.trim() : "";
    if (value != this.alias) {
      this.setAttr("alias", value);
      lazy.SearchUtils.notifyAction(
        this,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );
    }
  }

  /**
   * Returns a list of aliases, including a user defined alias and
   * a list defined by webextension keywords.
   *
   * @returns {Array}
   */
  get aliases() {
    return [
      ...(this.getAttr("alias") ? [this.getAttr("alias")] : []),
      ...this._definedAliases,
    ];
  }

  /**
   * Returns the appropriate identifier to use for telemetry. It is based on
   * the following order:
   *
   * - telemetryId: The telemetry id from the configuration, or derived from
   *                the WebExtension name.
   * - other-<name>: The engine name prefixed by `other-` for non-app-provided
   *                 engines.
   *
   * @returns {string}
   */
  get telemetryId() {
    let telemetryId = this._telemetryId || `other-${this.name}`;
    if (this.getAttr("overriddenBy")) {
      return telemetryId + "-addon";
    }
    return telemetryId;
  }

  /**
   * Return the built-in identifier of app-provided engines.
   *
   * @returns {string|null}
   *   Returns a valid if this is a built-in engine, null otherwise.
   */
  get identifier() {
    // No identifier if If the engine isn't app-provided
    return this.isAppProvided ? this._telemetryId : null;
  }

  get description() {
    return this._description;
  }

  get hidden() {
    return this.getAttr("hidden") || false;
  }
  set hidden(val) {
    var value = !!val;
    if (value != this.hidden) {
      this.setAttr("hidden", value);
      lazy.SearchUtils.notifyAction(
        this,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );
    }
  }

  get hideOneOffButton() {
    return this.getAttr("hideOneOffButton") || false;
  }
  set hideOneOffButton(val) {
    const value = !!val;
    if (value != this.hideOneOffButton) {
      this.setAttr("hideOneOffButton", value);
      lazy.SearchUtils.notifyAction(
        this,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );
    }
  }

  get _iconURL() {
    if (!this._iconURI) {
      return "";
    }
    return this._iconURI.spec;
  }

  // Where the engine is being loaded from: will return the URI's spec if the
  // engine is being downloaded and does not yet have a file. This is only used
  // for logging and error messages.
  get _location() {
    if (this._uri) {
      return this._uri.spec;
    }

    return this._loadPath;
  }

  /**
   * Whether or not this engine is provided by the application, e.g. it is
   * in the list of configured search engines.
   *
   * @returns {boolean}
   *   This returns false for most engines, but may be overridden by particular
   *   engine types, such as add-on engines which are used by the application.
   */
  get isAppProvided() {
    return false;
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
   *   This results false for most engines, but may be overridden by particular
   *   engine types, such as add-on engines and policy engines.
   */
  get inMemory() {
    return false;
  }

  get isGeneralPurposeEngine() {
    return false;
  }

  get _hasUpdates() {
    return false;
  }

  get name() {
    return this._name;
  }

  /**
   * The searchForm URL points to the engine's organic search page. This should
   * not contain neither search term parameters nor partner codes, but may
   * contain parameters which set the engine in the correct way.
   *
   * This URL is typically the prePath and filePath of the search submission URI,
   * but may vary for different engines. For example, some engines may use a
   * different domain, e.g. https://sub.example.com for the search URI but
   * https://example.org/ for the organic search page.
   *
   * @returns {string}
   */
  get searchForm() {
    // First look for a <Url rel="searchform">
    var searchFormURL = this._getURLOfType(
      lazy.SearchUtils.URL_TYPE.SEARCH,
      "searchform"
    );
    if (searchFormURL) {
      let submission = searchFormURL.getSubmission("", this);

      // If the rel=searchform URL is not type="get" (i.e. has postData),
      // ignore it, since we can only return a URL.
      if (!submission.postData) {
        return submission.uri.spec;
      }
    }

    if (!this._searchForm) {
      // No SearchForm specified in the engine definition file, use the prePath
      // (e.g. https://foo.com for https://foo.com/search.php?q=bar).
      var htmlUrl = this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH);
      if (!htmlUrl) {
        throw Components.Exception(
          "Engine has no HTML URL!",
          Cr.NS_ERROR_UNEXPECTED
        );
      }
      this._searchForm = lazy.SearchUtils.makeURI(htmlUrl.template).prePath;
    }

    return ParamSubstitution(this._searchForm, "", this);
  }

  get queryCharset() {
    return this._queryCharset || lazy.SearchUtils.DEFAULT_QUERY_CHARSET;
  }

  /**
   * Gets an object that contains information about what to send to the search
   * engine, for a request. This will be a URI and may also include data for POST
   * requests.
   *
   * @param {string} searchTerms
   *   The search term(s) for the submission.
   *   Note: If an empty data string is supplied, the search form of the search
   *   engine will be returned. This is intentional, as in some cases on the current
   *   UI an empty search is intended to open the search engine's home/search page.
   * @param {lazy.SearchUtils.URL_TYPE} [responseType]
   *   The MIME type that we'd like to receive in response
   *   to this submission.  If null, will default to "text/html".
   * @param {string} [purpose]
   *   A string that indicates the context of the search request. This may then
   *   be used to provide different submission data depending on the context.
   * @returns {nsISearchSubmission|null}
   *   The submission data. If no appropriate submission can be determined for
   *   the request type, this may be null.
   */
  getSubmission(searchTerms, responseType, purpose) {
    // We can't use a default parameter as that doesn't work correctly with
    // the idl interfaces.
    if (!responseType) {
      responseType = lazy.SearchUtils.URL_TYPE.SEARCH;
    }

    var url = this._getURLOfType(responseType);

    if (!url) {
      return null;
    }

    if (
      !searchTerms &&
      responseType != lazy.SearchUtils.URL_TYPE.TRENDING_JSON
    ) {
      // Return a dummy submission object with our searchForm attribute
      return new Submission(lazy.SearchUtils.makeURI(this.searchForm));
    }

    var submissionData = "";
    try {
      submissionData = Services.textToSubURI.ConvertAndEscape(
        this.queryCharset,
        searchTerms
      );
    } catch (ex) {
      lazy.logConsole.warn(
        "getSubmission: Falling back to default queryCharset!"
      );
      submissionData = Services.textToSubURI.ConvertAndEscape(
        lazy.SearchUtils.DEFAULT_QUERY_CHARSET,
        searchTerms
      );
    }
    return url.getSubmission(submissionData, this, purpose);
  }

  /**
   * Returns a search URL with no search terms. This is typically used for
   * purposes where we want to check something on the URL, but not use it for
   * an actual submission to the search engine.
   *
   * Note: getSubmission cannot be used for this case, as that returns the
   * search form when passed an empty string.
   *
   * @returns {nsIURI}
   */
  get searchURLWithNoTerms() {
    return this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH).getSubmission(
      "",
      this
    ).uri;
  }

  /**
   * Returns the search term of a possible search result URI if and only if:
   * - The URI has the same scheme, host, and path as the engine.
   * - All query parameters of the URI have a matching name and value in the engine.
   * - An exception to the equality check is the engine's termsParameterName
   *   value, which contains a placeholder, i.e. {searchTerms}.
   * - If an engine has query parameters with "null" values, they will be ignored.
   *
   * @param {nsIURI} uri
   *   A URI that may or may not be from a search result matching the engine.
   *
   * @returns {string}
   *   A string representing the termsParameterName value of the URI,
   *   or an empty string if the URI isn't matched to the engine.
   */
  searchTermFromResult(uri) {
    let url = this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH);
    if (!url) {
      return "";
    }

    // The engine URL and URI should have the same scheme, host, and path.
    if (
      uri.spec.split("?")[0].toLowerCase() !=
      url.template.split("?")[0].toLowerCase()
    ) {
      return "";
    }

    let engineParams;
    if (url.params.length) {
      engineParams = new URLSearchParams();
      for (let { name, value } of url.params) {
        // Some values might be null, so avoid adding
        // them since the input is unlikely to have it too.
        if (value) {
          // Use append() rather than set() so multiple
          // values of the same name can be stored.
          engineParams.append(name, value);
        }
      }
    } else {
      // Try checking the template for the presence of query params.
      engineParams = new URL(url.template).searchParams;
    }

    let uriParams = new URLSearchParams(uri.query);
    if (
      new Set([...uriParams.keys()]).size !=
      new Set([...engineParams.keys()]).size
    ) {
      return "";
    }

    let termsParameterName = this.getURLParsingInfo().termsParameterName;
    for (let [name, value] of uriParams.entries()) {
      // Don't check the name matching the search
      // query because its value will differ.
      if (name == termsParameterName) {
        continue;
      }
      // All params of an input must have a matching
      // key and value in the list of engine parameters.
      if (!engineParams.getAll(name).includes(value)) {
        return "";
      }
    }

    // An engine can use a non UTF-8 charset, which URLSearchParams
    // might not parse properly. Convert the terms parameter value
    // from the original input using the appropriate charset.
    if (this.queryCharset.toLowerCase() != "utf-8") {
      let name = `${termsParameterName}=`;
      let queryString = uri.query
        .split("&")
        .filter(str => str.startsWith(name))
        .pop();
      return Services.textToSubURI.UnEscapeAndConvert(
        this.queryCharset,
        queryString.substring(queryString.indexOf("=") + 1).replace(/\+/g, " ")
      );
    }

    return uriParams.get(termsParameterName);
  }

  get searchUrlQueryParamName() {
    if (this._searchUrlQueryParamName != null) {
      return this._searchUrlQueryParamName;
    }

    let submission = this.getSubmission(
      "{searchTerms}",
      lazy.SearchUtils.URL_TYPE.SEARCH
    );

    if (submission.postData) {
      console.error("searchUrlQueryParamName can't handle POST urls.");
      return (this._searchUrlQueryParamName = "");
    }

    let queryParams = new URLSearchParams(submission.uri.query);
    let searchUrlQueryParamName = "";
    for (let [key, value] of queryParams) {
      if (value == "{searchTerms}") {
        searchUrlQueryParamName = key;
      }
    }

    return (this._searchUrlQueryParamName = searchUrlQueryParamName);
  }

  get searchUrlPublicSuffix() {
    if (this._searchUrlPublicSuffix != null) {
      return this._searchUrlPublicSuffix;
    }
    let searchURLPublicSuffix = Services.eTLD.getKnownPublicSuffix(
      this.searchURLWithNoTerms
    );
    return (this._searchUrlPublicSuffix = searchURLPublicSuffix);
  }

  // from nsISearchEngine
  supportsResponseType(type) {
    return this._getURLOfType(type) != null;
  }

  // from nsISearchEngine
  get searchUrlDomain() {
    let url = this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH);
    if (url) {
      return url.templateHost;
    }
    return "";
  }

  /**
   * @returns {object}
   *   URL parsing properties used by _buildParseSubmissionMap.
   */
  getURLParsingInfo() {
    let url = this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH);
    if (!url || url.method != "GET") {
      return null;
    }

    let termsParameterName = url._getTermsParameterName();
    if (!termsParameterName) {
      return null;
    }

    let templateUrl = Services.io.newURI(url.template);
    return {
      mainDomain: templateUrl.host,
      path: templateUrl.filePath.toLowerCase(),
      termsParameterName,
    };
  }

  get wrappedJSObject() {
    return this;
  }

  /**
   * Retrieves the icon URL for this search engine, if any.
   *
   * @param {number} preferredWidth
   *   Width of the requested icon. If not specified, it is assumed that
   *   16x16 is desired.
   * @returns {string|undefined}
   */
  getIconURL(preferredWidth) {
    // XPCOM interfaces pass optional number parameters as 0 and can't be
    // handled in the same way.
    if (!preferredWidth) {
      preferredWidth = 16;
    }

    if (preferredWidth == 16) {
      return this._iconURL || undefined;
    }

    if (!this._iconMapObj) {
      return undefined;
    }

    let key = this._getIconKey(preferredWidth, preferredWidth);
    if (key in this._iconMapObj) {
      return this._iconMapObj[key];
    }
    return undefined;
  }

  /**
   * Opens a speculative connection to the engine's search URI
   * (and suggest URI, if different) to reduce request latency
   *
   * @param {object} options
   *   The options object
   * @param {DOMWindow} options.window
   *   The content window for the window performing the search.
   * @param {object} options.originAttributes
   *   The originAttributes for performing the search
   * @throws NS_ERROR_INVALID_ARG if options is omitted or lacks required
   *         elements
   */
  speculativeConnect(options) {
    if (!options || !options.window) {
      console.error(
        "invalid options arg passed to nsISearchEngine.speculativeConnect"
      );
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }
    let connector = Services.io.QueryInterface(Ci.nsISpeculativeConnect);

    let searchURI = this.searchURLWithNoTerms;

    let callbacks = options.window.docShell.QueryInterface(Ci.nsILoadContext);

    // Using the content principal which is constructed by the search URI
    // and given originAttributes. If originAttributes are not given, we
    // fallback to use the docShell's originAttributes.
    let attrs = options.originAttributes;

    if (!attrs) {
      attrs = options.window.docShell.getOriginAttributes();
    }

    let principal = Services.scriptSecurityManager.createContentPrincipal(
      searchURI,
      attrs
    );

    try {
      connector.speculativeConnect(searchURI, principal, callbacks, false);
    } catch (e) {
      // Can't setup speculative connection for this url, just ignore it.
      console.error(e);
    }

    if (this.supportsResponseType(lazy.SearchUtils.URL_TYPE.SUGGEST_JSON)) {
      let suggestURI = this.getSubmission(
        "dummy",
        lazy.SearchUtils.URL_TYPE.SUGGEST_JSON
      ).uri;
      if (suggestURI.prePath != searchURI.prePath) {
        try {
          connector.speculativeConnect(suggestURI, principal, callbacks, false);
        } catch (e) {
          // Can't setup speculative connection for this url, just ignore it.
          console.error(e);
        }
      }
    }
  }

  get id() {
    return this.#id;
  }

  /**
   * Generates an UUID.
   *
   * @returns {string}
   *   An UUID string, without leading or trailing braces.
   */
  #uuid() {
    let uuid = Services.uuid.generateUUID().toString();
    return uuid.slice(1, uuid.length - 1);
  }
}

/**
 * Implements nsISearchSubmission.
 */
class Submission {
  QueryInterface = ChromeUtils.generateQI(["nsISearchSubmission"]);

  constructor(uri, postData = null) {
    this._uri = uri;
    this._postData = postData;
  }

  get uri() {
    return this._uri;
  }
  get postData() {
    return this._postData;
  }
}
