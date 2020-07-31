/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  Region: "resource://gre/modules/Region.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gModernConfig",
  SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
  false
);

XPCOMUtils.defineLazyGetter(this, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchEngine",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
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
 * @param {array} byteArray
 *   Byte array containing the icon payload.
 * @param {string} contentType
 *   Mime type of the payload.
 * @param {number} [size]
 *   Desired icon size.
 * @returns {array}
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
  if (streamSize > SearchUtils.MAX_ICON_SIZE) {
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
    this.branch = Services.prefs.getDefaultBranch(
      SearchUtils.BROWSER_SEARCH_PREF + "param."
    );
    this.cache = new Map();
    for (let prefName of this.branch.getChildList("")) {
      this.cache.set(prefName, this.branch.getCharPref(prefName, null));
    }
    this.branch.addObserver("", this, true);
  },

  observe(subject, topic, data) {
    this.cache.set(data, this.branch.getCharPref(data, null));
  },

  getPref(prefName) {
    if (!this.cache) {
      this.initCache();
    }
    return this.cache.get(prefName);
  },
};

/**
 * Represents a name/value pair for a parameter
 * @see nsISearchEngine::addParam
 */
class QueryParameter {
  /**
   * @see nsISearchEngine::addParam
   * @param {string} name
   * @param {string} value
   *  The value of the parameter. May be an empty string, must not be null or
   *  undefined.
   * @param {string} purpose
   *   The search purpose for which matches when this parameter should be
   *   applied, e.g. "searchbar", "contextmenu".
   */
  constructor(name, value, purpose) {
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
  return paramValue.replace(PARAM_REGEXP, function(match, name, optional) {
    // {searchTerms} is by far the most common param so handle it first.
    if (name == USER_DEFINED) {
      return searchTerms;
    }

    // {inputEncoding} is the second most common param.
    if (name == OS_PARAM_INPUT_ENCODING) {
      return engine.queryCharset;
    }

    // moz: parameters are only available for default search engines.
    if (name.startsWith("moz:") && engine.isAppProvided) {
      // {moz:locale} is common.
      if (name == SearchUtils.MOZ_PARAM.LOCALE) {
        return Services.locale.requestedLocale;
      }

      // {moz:date}
      if (name == SearchUtils.MOZ_PARAM.DATE) {
        let date = new Date();
        let pad = number => number.toString().padStart(2, "0");
        return (
          String(date.getFullYear()) +
          pad(date.getMonth() + 1) +
          pad(date.getDate()) +
          pad(date.getHours())
        );
      }

      // {moz:distributionID} and {moz:official} seem to have little use.
      if (name == SearchUtils.MOZ_PARAM.DIST_ID) {
        return Services.prefs.getCharPref(
          SearchUtils.BROWSER_SEARCH_PREF + "distributionID",
          Services.appinfo.distributionID || ""
        );
      }

      if (name == SearchUtils.MOZ_PARAM.OFFICIAL) {
        if (
          Services.prefs.getBoolPref(
            SearchUtils.BROWSER_SEARCH_PREF + "official",
            AppConstants.MOZ_OFFICIAL_BRANDING
          )
        ) {
          return "official";
        }
        return "unofficial";
      }
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

const ENGINE_ALIASES = new Map([
  ["google", ["@google"]],
  ["amazondotcom", ["@amazon"]],
  ["amazon", ["@amazon"]],
  ["wikipedia", ["@wikipedia"]],
  ["ebay", ["@ebay"]],
  ["bing", ["@bing"]],
  ["ddg", ["@duckduckgo", "@ddg"]],
  ["yandex", ["@\u044F\u043D\u0434\u0435\u043A\u0441", "@yandex"]],
  ["baidu", ["@\u767E\u5EA6", "@baidu"]],
]);

function getInternalAliases(engine) {
  if (!engine.isAppProvided) {
    return [];
  }
  for (let [name, aliases] of ENGINE_ALIASES) {
    // This may match multiple engines (amazon vs amazondotcom), they
    // shouldn't be installed together but if they are the first
    // is picked.
    if (engine._shortName.startsWith(name)) {
      return aliases;
    }
  }
  return [];
}

/**
 * EngineURL holds a query URL and all associated parameters.
 */
class EngineURL {
  params = [];
  rels = [];

  /**
   * Constructor
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
   * @param {string} [resultDomain]
   *   The root domain for this URL.  Defaults to the template's host.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
   *
   * @throws NS_ERROR_NOT_IMPLEMENTED if aType is unsupported.
   */
  constructor(mimeType, requestMethod, template, resultDomain) {
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

    var templateURI = SearchUtils.makeURI(template);
    if (!templateURI) {
      throw Components.Exception(
        "new EngineURL: template is not a valid URI!",
        Cr.NS_ERROR_FAILURE
      );
    }

    switch (templateURI.scheme) {
      case "http":
      case "https":
        // Disable these for now, see bug 295018
        // case "file":
        // case "resource":
        this.template = template;
        break;
      default:
        throw Components.Exception(
          "new EngineURL: template uses invalid scheme!",
          Cr.NS_ERROR_FAILURE
        );
    }

    this.templateHost = templateURI.host;
    // If no resultDomain was specified in the engine definition file, use the
    // host from the template.
    this.resultDomain = resultDomain || this.templateHost;
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

      let paramValue = param.value;
      // Override the parameter value if the engine has a region
      // override defined for our current region.
      if (engine._regionParams?.[Region.current]) {
        let override = engine._regionParams[Region.current].find(
          p => p.name == param.name
        );
        if (override) {
          paramValue = override.value;
        }
      }
      // Preference MozParams might not have a preferenced saved, or a valid value.
      if (paramValue != null) {
        var value = ParamSubstitution(paramValue, searchTerms, engine);

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
    let queryParam = this.params.find(p => p.value == "{" + USER_DEFINED + "}");
    return queryParam ? queryParam.name : "";
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
      if (param.mozparam) {
        this._addMozParam(param);
      } else {
        this.addParam(param.name, param.value, param.purpose || undefined);
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
      resultDomain: this.resultDomain,
      template: this.template,
    };

    if (this.type != SearchUtils.URL_TYPE.SEARCH) {
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
class SearchEngine {
  QueryInterface = ChromeUtils.generateQI(["nsISearchEngine"]);
  // Data set by the user.
  _metaData = {};
  // Anonymized path of where we initially loaded the engine from.
  // This will stay null for engines installed in the profile before we moved
  // to a JSON storage.
  _loadPath = null;
  // The engine's description
  _description = "";
  // Used to store the engine to replace, if we're an update to an existing
  // engine.
  _engineToUpdate = null;
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon = null;
  // The engine's name.
  _name = null;
  // The name of the charset used to submit the search terms.
  _queryCharset = null;
  // The engine's raw SearchForm value (URL string pointing to a search form).
  __searchForm = null;
  // The number of days between update checks for new versions
  _updateInterval = null;
  // The url to check at for a new update
  _updateURL = null;
  // The url to check for a new icon
  _iconUpdateURL = null;
  // The extension ID if added by an extension.
  _extensionID = null;
  // The locale, or "DEFAULT", if required.
  _locale = null;
  // Whether the engine is provided by the application.
  _isAppProvided = false;
  // The order hint from the configuration (if any).
  _orderHint = null;
  // The telemetry id from the configuration (if any).
  _telemetryId = null;
  // Set to true once the engine has been added to the store, and the initial
  // notification sent. This allows to skip sending notifications during
  // initialization.
  _engineAddedToStore = false;
  // The alias coming from the engine definition (via webextension
  // keyword field for example) may be overridden in the metaData
  // with a user defined alias.
  _definedAlias = null;
  // The urls associated with this engine.
  _urls = [];
  // Internal aliases for default engines only.
  __internalAliases = null;

  /**
   * Constructor.
   *
   * @param {object} options
   *   The options for this search engine. At least one of options.name,
   *   or options.shortName are required.
   * @param {string} [options.name]
   *   The name to base the short name of the engine on. This is typically the
   *   display name where a pre-defined/sanitized short name is not available.
   * @param {string} [options.shortName]
   *   The short name to use for the engine. This should be known to match
   *   the basic requirements in sanitizeName for a short name.
   * @param {boolean} options.isAppProvided
   *   Indicates whether the engine is provided by Firefox, either
   *   shipped in omni.ja or via Normandy. If it is, it will
   *   be treated as read-only.
   * @param {string} options.loadPath
   *   The path of the engine was originally loaded from. Should be anonymized.
   */
  constructor(options = {}) {
    if (!("isAppProvided" in options)) {
      throw new Error("isAppProvided missing from options.");
    }
    if (!("loadPath" in options)) {
      throw new Error("loadPath missing from options.");
    }
    this._isAppProvided = options.isAppProvided;
    this._loadPath = options.loadPath;

    if ("name" in options) {
      this._shortName = SearchUtils.sanitizeName(options.name);
    } else if ("shortName" in options) {
      this._shortName = options.shortName;
    } else {
      throw new Error("'name' or 'shortName' missing from options.");
    }
  }

  get _searchForm() {
    return this.__searchForm;
  }
  set _searchForm(value) {
    if (/^https?:/i.test(value)) {
      this.__searchForm = value;
    } else {
      logConsole.debug(
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
   * Add an icon to the icon map used by getIconURIBySize() and getIcons().
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
   * icon's data through getIcons() and getIconURIBySize() APIs.
   *
   * @param {string} iconURL
   *   A URI string pointing to the engine's icon. Must have a http[s],
   *   ftp, or data scheme. Icons with HTTP[S] or FTP schemes will be
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
    var uri = SearchUtils.makeURI(iconURL);

    // Ignore bad URIs
    if (!uri) {
      return;
    }

    logConsole.debug(
      "_setIcon: Setting icon url for",
      this.name,
      "to",
      limitURILength(uri.spec)
    );
    // Only accept remote icons from http[s] or ftp
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
      case "ftp":
        var chan = SearchUtils.makeChannel(uri);

        let iconLoadCallback = function(byteArray, engine) {
          // This callback may run after we've already set a preferred icon,
          // so check again.
          if (engine._hasPreferredIcon && !isPreferred) {
            return;
          }

          if (!byteArray) {
            logConsole.warn("iconLoadCallback: load failed");
            return;
          }

          let contentType = chan.contentType;
          if (byteArray.length > SearchUtils.MAX_ICON_SIZE) {
            try {
              logConsole.debug("iconLoadCallback: rescaling icon");
              [byteArray, contentType] = rescaleIcon(byteArray, contentType);
            } catch (ex) {
              logConsole.error("Unable to set icon for the search engine:", ex);
              return;
            }
          }

          if (!contentType.startsWith("image/")) {
            contentType = "image/x-icon";
          }
          let dataURL =
            "data:" +
            contentType +
            ";base64," +
            btoa(String.fromCharCode.apply(null, byteArray));

          engine._iconURI = SearchUtils.makeURI(dataURL);

          if (width && height) {
            engine._addIconToMap(width, height, dataURL);
          }

          if (engine._engineAddedToStore) {
            SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.CHANGED);
          }
          engine._hasPreferredIcon = isPreferred;
        };

        // If we're currently acting as an "update engine", then the callback
        // should set the icon on the engine we're updating and not us, since
        // |this| might be gone by the time the callback runs.
        var engineToSet = this._engineToUpdate || this;

        var listener = new SearchUtils.LoadListener(
          chan,
          engineToSet,
          iconLoadCallback
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
   * @param {string|array} [params.getParams]
   *   Any parameters for a GET method. This is either a query string, or
   *   an array of objects which have name/value pairs.
   * @param {string} [params.method]
   *   The type of method, defaults to GET.
   * @param {string} [params.mozParams]
   *   Any special Mozilla Parameters.
   * @param {string|array} [params.postParams]
   *   Any parameters for a POST method. This is either a query string, or
   *   an array of objects which have name/value pairs.
   * @param {string} params.template
   *   The url template.
   */
  _initEngineURLFromMetaData(type, params) {
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

    this._urls.push(url);
  }

  /**
   * Initialize this Engine object from a WebExtension style manifest.
   *
   * @param {string} extensionID
   *   The WebExtension ID. For Policy engines, this is currently "set-via-policy".
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
  _initFromManifest(
    extensionID,
    extensionBaseURI,
    manifest,
    locale,
    configuration = {}
  ) {
    let { IconDetails } = ExtensionParent;

    let searchProvider = manifest.chrome_settings_overrides.search_provider;

    let iconURL = manifest.iconURL || searchProvider.favicon_url;

    // General set of icons for an engine.
    let icons = manifest.icons;
    let iconList = [];
    if (icons) {
      iconList = Object.entries(icons).map(icon => {
        return {
          width: icon[0],
          height: icon[0],
          url: extensionBaseURI.resolve(icon[1]),
        };
      });
    }

    if (!iconURL) {
      iconURL =
        icons &&
        extensionBaseURI.resolve(IconDetails.getPreferredIcon(icons).icon);
    }

    let shortName = extensionID.split("@")[0];
    if (locale != SearchUtils.DEFAULT_TAG) {
      shortName += "-" + locale;
    }
    // TODO: Bug 1619656. We should no longer need to maintain the short name as
    // the telemetry id. However, we need to check that this doesn't adversely
    // affect settings or caches.
    if ("telemetryId" in configuration && configuration.telemetryId) {
      shortName = configuration.telemetryId;
    }

    this._extensionID = extensionID;
    this._locale = locale;
    this._orderHint = configuration.orderHint;
    this._telemetryId = configuration.telemetryId;
    this._name = searchProvider.name.trim();
    this._regionParams = configuration.regionParams;

    if (shortName) {
      this._shortName = shortName;
    }
    this._definedAlias = searchProvider.keyword?.trim() || null;
    this._description = manifest.description;
    if (iconURL) {
      this._setIcon(iconURL, true);
    }
    // Other sizes
    if (iconList) {
      for (let icon of iconList) {
        this._addIconToMap(icon.size, icon.size, icon.url);
      }
    }
    this._setUrls(searchProvider, configuration);
  }

  /**
   * This sets the urls for the search engine based on the supplied parameters.
   * If you add anything here, please consider if it needs to be handled in the
   * overrideWithExtension / removeExtensionOverride functions as well.
   *
   * @param {object} searchProvider
   *   The WebExtension search provider object extracted from the manifest.
   * @param {object} [configuration]
   *   The search engine configuration for application provided engines, that
   *   may be overriding some of the WebExtension's settings.
   */
  _setUrls(searchProvider, configuration = {}) {
    // Filter out any untranslated parameters, the extension has to list all
    // possible mozParams for each engine where a 'locale' may only provide
    // actual values for some (or none).
    if (searchProvider.params) {
      searchProvider.params = searchProvider.params.filter(param => {
        return !(param.value && param.value.startsWith("__MSG_"));
      });
    }

    let postParams =
      configuration.params?.searchUrlPostParams ||
      searchProvider.search_url_post_params ||
      "";
    this._initEngineURLFromMetaData(SearchUtils.URL_TYPE.SEARCH, {
      method: (postParams && "POST") || "GET",
      // AddonManager will sometimes encode the URL via `new URL()`. We want
      // to ensure we're always dealing with decoded urls.
      template: decodeURI(searchProvider.search_url),
      getParams:
        configuration.params?.searchUrlGetParams ||
        searchProvider.search_url_get_params ||
        "",
      postParams,
      mozParams: configuration.extraParams || searchProvider.params || [],
    });

    if (searchProvider.suggest_url) {
      let suggestPostParams =
        configuration.params?.suggestUrlPostParams ||
        searchProvider.suggest_url_post_params ||
        "";
      this._initEngineURLFromMetaData(SearchUtils.URL_TYPE.SUGGEST_JSON, {
        method: (suggestPostParams && "POST") || "GET",
        // suggest_url doesn't currently get encoded.
        template: searchProvider.suggest_url,
        getParams:
          configuration.params?.suggestUrlGetParams ||
          searchProvider.suggest_url_get_params ||
          "",
        postParams: suggestPostParams,
      });
    }

    this._queryCharset = searchProvider.encoding || "UTF-8";
    this.__searchForm = searchProvider.search_form;
  }

  /**
   * Update this engine based on new manifest, used during
   * webextension upgrades.
   *
   * @param {string} extensionID
   *   The WebExtension ID. For Policy engines, this is currently "set-via-policy".
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
  _updateFromManifest(
    extensionID,
    extensionBaseURI,
    manifest,
    locale,
    configuration = {}
  ) {
    this._urls = [];
    this._iconMapObj = null;
    this._initFromManifest(
      extensionID,
      extensionBaseURI,
      manifest,
      locale,
      configuration
    );
    SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Overrides the urls/parameters with those of the provided extension.
   * The parameters are not saved to the search cache - the code handling
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
      searchForm: this.__searchForm,
    };
    this._urls = [];
    this.setAttr("overriddenBy", extensionID);
    this._setUrls(manifest.chrome_settings_overrides.search_provider);
    SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Resets the overrides for the engine if it has been overridden.
   */
  removeExtensionOverride() {
    if (this.getAttr("overriddenBy")) {
      this._urls = this._overriddenData.urls;
      this._queryCharset = this._overriddenData.queryCharset;
      this.__searchForm = this._overriddenData.searchForm;
      delete this._overriddenData;
      this.clearAttr("overriddenBy");
      SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
    }
  }

  /**
   * Init from a JSON record.
   *
   * @param {object} json
   *   The json record to use.
   */
  _initWithJSON(json) {
    this._name = json._name;
    this._shortName = json._shortName;
    this._description = json.description;
    this._hasPreferredIcon = json._hasPreferredIcon == undefined;
    this._queryCharset = json.queryCharset || SearchUtils.DEFAULT_QUERY_CHARSET;
    this.__searchForm = json.__searchForm;
    this._updateInterval = json._updateInterval || null;
    this._updateURL = json._updateURL || null;
    this._iconUpdateURL = json._iconUpdateURL || null;
    this._iconURI = SearchUtils.makeURI(json._iconURL);
    this._iconMapObj = json._iconMapObj;
    this._metaData = json._metaData || {};
    this._orderHint = json._orderHint || null;
    this._telemetryId = json._telemetryId || null;
    this._definedAlias = json._definedAlias || null;
    // These changed keys in Firefox 80, maintain the old keys
    // for backwards compatibility.
    this._filePath = json.filePath || json._filePath || null;
    this._extensionID = json.extensionID || json._extensionID || null;
    this._locale = json.extensionLocale || json._locale || null;

    for (let i = 0; i < json._urls.length; ++i) {
      let url = json._urls[i];
      let engineURL = new EngineURL(
        url.type || SearchUtils.URL_TYPE.SEARCH,
        url.method || "GET",
        url.template,
        url.resultDomain || undefined
      );
      engineURL._initWithJSON(url);
      this._urls.push(engineURL);
    }
  }

  /**
   * Creates a JavaScript object that represents this engine.
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    // For built-in engines we don't want to store all their data in the cache
    // so just store the relevant metadata.
    if (gModernConfig && this._isAppProvided) {
      return {
        _name: this.name,
        _isAppProvided: true,
        _metaData: this._metaData,
      };
    }

    const fieldsToCopy = [
      "_name",
      "_shortName",
      "_loadPath",
      "description",
      "__searchForm",
      "_iconURL",
      "_iconMapObj",
      "_metaData",
      "_urls",
      "_isAppProvided",
      "_orderHint",
      "_telemetryId",
      "_updateInterval",
      "_updateURL",
      "_iconUpdateURL",
      "_filePath",
      "_extensionID",
      "_locale",
      "_definedAlias",
    ];

    let json = {};
    for (const field of fieldsToCopy) {
      if (field in this) {
        json[field] = this[field];
      }
    }

    if (!this._hasPreferredIcon) {
      json._hasPreferredIcon = this._hasPreferredIcon;
    }
    if (this.queryCharset != SearchUtils.DEFAULT_QUERY_CHARSET) {
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

  // nsISearchEngine
  get alias() {
    return this.getAttr("alias") || this._definedAlias;
  }
  set alias(val) {
    var value = val ? val.trim() : null;
    this.setAttr("alias", value);
    SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Returns the appropriate identifier to use for telemetry. It is based on
   * the following order:
   *
   * - telemetryId: The telemetry id from the configuration.
   * - identifier: The built-in identifier of app-provided engines.
   * - other-<name>: The engine name prefixed by `other-` for non-app-provided
   *                 engines.
   *
   * @returns {string}
   */
  get telemetryId() {
    let telemetryId =
      this._telemetryId || this.identifier || `other-${this.name}`;
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
    return this.isAppProvided ? this._shortName : null;
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
      SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
    }
  }

  get iconURI() {
    if (this._iconURI) {
      return this._iconURI;
    }
    return null;
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

  get _isDistribution() {
    return !!(
      this._extensionID &&
      Services.prefs.getCharPref(
        `extensions.installedDistroAddon.${this._extensionID}`,
        ""
      )
    );
  }

  get isAppProvided() {
    // For the modern configuration, distribution engines are app-provided as
    // well and we don't have xml files as app-provided engines.
    if (gModernConfig) {
      return !!(this._extensionID && this._isAppProvided);
    }

    if (this._extensionID) {
      return this._isAppProvided || this._isDistribution;
    }

    // If we don't have a shortName, the engine is being parsed from a
    // downloaded file, so this can't be a default engine.
    if (!this._shortName) {
      return false;
    }

    // An engine is a default one if we initially loaded it from the application
    // or distribution directory.
    if (/^(?:jar:)?(?:\[app\]|\[distribution\])/.test(this._loadPath)) {
      return true;
    }

    return false;
  }

  get _hasUpdates() {
    return false;
  }

  get name() {
    return this._name;
  }

  get searchForm() {
    return this._getSearchFormWithPurpose();
  }

  get _internalAliases() {
    if (!this.__internalAliases) {
      this.__internalAliases = getInternalAliases(this);
    }
    return this.__internalAliases;
  }

  _getSearchFormWithPurpose(purpose) {
    // First look for a <Url rel="searchform">
    var searchFormURL = this._getURLOfType(
      SearchUtils.URL_TYPE.SEARCH,
      "searchform"
    );
    if (searchFormURL) {
      let submission = searchFormURL.getSubmission("", this, purpose);

      // If the rel=searchform URL is not type="get" (i.e. has postData),
      // ignore it, since we can only return a URL.
      if (!submission.postData) {
        return submission.uri.spec;
      }
    }

    if (!this._searchForm) {
      // No SearchForm specified in the engine definition file, use the prePath
      // (e.g. https://foo.com for https://foo.com/search.php?q=bar).
      var htmlUrl = this._getURLOfType(SearchUtils.URL_TYPE.SEARCH);
      if (!htmlUrl) {
        throw Components.Exception(
          "Engine has no HTML URL!",
          Cr.NS_ERROR_UNEXPECTED
        );
      }
      this._searchForm = SearchUtils.makeURI(htmlUrl.template).prePath;
    }

    return ParamSubstitution(this._searchForm, "", this);
  }

  get queryCharset() {
    if (this._queryCharset) {
      return this._queryCharset;
    }
    return (this._queryCharset = "windows-1252"); // the default
  }

  get _defaultMobileResponseType() {
    let type = SearchUtils.URL_TYPE.SEARCH;

    let isTablet = Services.sysinfo.get("tablet");
    if (
      isTablet &&
      this.supportsResponseType("application/x-moz-tabletsearch")
    ) {
      // Check for a tablet-specific search URL override
      type = "application/x-moz-tabletsearch";
    } else if (
      !isTablet &&
      this.supportsResponseType("application/x-moz-phonesearch")
    ) {
      // Check for a phone-specific search URL override
      type = "application/x-moz-phonesearch";
    }

    Object.defineProperty(this, "_defaultMobileResponseType", {
      value: type,
      configurable: true,
    });

    return type;
  }

  // from nsISearchEngine
  getSubmission(data, responseType, purpose) {
    if (!responseType) {
      responseType =
        AppConstants.platform == "android"
          ? this._defaultMobileResponseType
          : SearchUtils.URL_TYPE.SEARCH;
    }

    var url = this._getURLOfType(responseType);

    if (!url) {
      return null;
    }

    if (!data) {
      // Return a dummy submission object with our searchForm attribute
      return new Submission(
        SearchUtils.makeURI(this._getSearchFormWithPurpose(purpose))
      );
    }

    var submissionData = "";
    try {
      submissionData = Services.textToSubURI.ConvertAndEscape(
        this.queryCharset,
        data
      );
    } catch (ex) {
      logConsole.warn("getSubmission: Falling back to default queryCharset!");
      submissionData = Services.textToSubURI.ConvertAndEscape(
        SearchUtils.DEFAULT_QUERY_CHARSET,
        data
      );
    }
    return url.getSubmission(submissionData, this, purpose);
  }

  // from nsISearchEngine
  supportsResponseType(type) {
    return this._getURLOfType(type) != null;
  }

  // from nsISearchEngine
  getResultDomain(responseType) {
    if (!responseType) {
      responseType =
        AppConstants.platform == "android"
          ? this._defaultMobileResponseType
          : SearchUtils.URL_TYPE.SEARCH;
    }

    let url = this._getURLOfType(responseType);
    if (url) {
      return url.resultDomain;
    }
    return "";
  }

  /**
   * @returns {object}
   *   URL parsing properties used by _buildParseSubmissionMap.
   */
  getURLParsingInfo() {
    let responseType =
      AppConstants.platform == "android"
        ? this._defaultMobileResponseType
        : SearchUtils.URL_TYPE.SEARCH;

    let url = this._getURLOfType(responseType);
    if (!url || url.method != "GET") {
      return null;
    }

    let termsParameterName = url._getTermsParameterName();
    if (!termsParameterName) {
      return null;
    }

    let templateUrl = Services.io
      .newURI(url.template)
      .QueryInterface(Ci.nsIURL);
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
   * Returns a string with the URL to an engine's icon matching both width and
   * height. Returns null if icon with specified dimensions is not found.
   *
   * @param {number} width
   *   Width of the requested icon.
   * @param {number} height
   *   Height of the requested icon.
   * @returns {string|null}
   */
  getIconURLBySize(width, height) {
    if (width == 16 && height == 16) {
      return this._iconURL;
    }

    if (!this._iconMapObj) {
      return null;
    }

    let key = this._getIconKey(width, height);
    if (key in this._iconMapObj) {
      return this._iconMapObj[key];
    }
    return null;
  }

  /**
   * Gets an array of all available icons. Each entry is an object with
   * width, height and url properties. width and height are numeric and
   * represent the icon's dimensions. url is a string with the URL for
   * the icon.
   *
   * @returns {Array<object>}
   *   An array of objects with width/height/url parameters.
   */
  getIcons() {
    let result = [];
    if (this._iconURL) {
      result.push({ width: 16, height: 16, url: this._iconURL });
    }

    if (!this._iconMapObj) {
      return result;
    }

    for (let key of Object.keys(this._iconMapObj)) {
      let iconSize = JSON.parse(key);
      result.push({
        width: iconSize.width,
        height: iconSize.height,
        url: this._iconMapObj[key],
      });
    }

    return result;
  }

  /**
   * Opens a speculative connection to the engine's search URI
   * (and suggest URI, if different) to reduce request latency
   *
   * @param {object} options
   * @param {DOMWindow} options.window
   *   The content window for the window performing the search.
   * @param {object} options.originAttributes
   *   The originAttributes for performing the search
   * @throws NS_ERROR_INVALID_ARG if options is omitted or lacks required
   *         elemeents
   */
  speculativeConnect(options) {
    if (!options || !options.window) {
      Cu.reportError(
        "invalid options arg passed to nsISearchEngine.speculativeConnect"
      );
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }
    let connector = Services.io.QueryInterface(Ci.nsISpeculativeConnect);

    let searchURI = this.getSubmission("dummy").uri;

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
      connector.speculativeConnect(searchURI, principal, callbacks);
    } catch (e) {
      // Can't setup speculative connection for this url, just ignore it.
      Cu.reportError(e);
    }

    if (this.supportsResponseType(SearchUtils.URL_TYPE.SUGGEST_JSON)) {
      let suggestURI = this.getSubmission(
        "dummy",
        SearchUtils.URL_TYPE.SUGGEST_JSON
      ).uri;
      if (suggestURI.prePath != searchURI.prePath) {
        try {
          connector.speculativeConnect(suggestURI, principal, callbacks);
        } catch (e) {
          // Can't setup speculative connection for this url, just ignore it.
          Cu.reportError(e);
        }
      }
    }
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

var EXPORTED_SYMBOLS = ["EngineURL", "SearchEngine"];
