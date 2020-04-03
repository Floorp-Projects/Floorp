/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  gEnvironment: ["@mozilla.org/process/environment;1", "nsIEnvironment"],
  gChromeReg: ["@mozilla.org/chrome/chrome-registry;1", "nsIChromeRegistry"],
});

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const SEARCH_BUNDLE = "chrome://global/locale/search/search.properties";
const BRAND_BUNDLE = "chrome://branding/locale/brand.properties";

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

const USER_DEFINED = "searchTerms";

// Custom search parameters
const MOZ_PARAM_LOCALE = "moz:locale";
const MOZ_PARAM_DIST_ID = "moz:distributionID";
const MOZ_PARAM_OFFICIAL = "moz:official";

// Supported OpenSearch parameters
// See http://opensearch.a9.com/spec/1.1/querysyntax/#core
const OS_PARAM_INPUT_ENCODING = "inputEncoding";
const OS_PARAM_LANGUAGE = "language";
const OS_PARAM_OUTPUT_ENCODING = "outputEncoding";

// Default values
const OS_PARAM_LANGUAGE_DEF = "*";
const OS_PARAM_OUTPUT_ENCODING_DEF = "UTF-8";
const OS_PARAM_INPUT_ENCODING_DEF = "UTF-8";

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

function loadListener(channel, engine, callback) {
  this._channel = channel;
  this._bytes = [];
  this._engine = engine;
  this._callback = callback;
}
loadListener.prototype = {
  _callback: null,
  _channel: null,
  _countRead: 0,
  _engine: null,
  _stream: null,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIRequestObserver,
    Ci.nsIStreamListener,
    Ci.nsIChannelEventSink,
    Ci.nsIInterfaceRequestor,
    Ci.nsIProgressEventSink,
  ]),

  // nsIRequestObserver
  onStartRequest(request) {
    SearchUtils.log("loadListener: Starting request: " + request.name);
    this._stream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
  },

  onStopRequest(request, statusCode) {
    SearchUtils.log("loadListener: Stopping request: " + request.name);

    var requestFailed = !Components.isSuccessCode(statusCode);
    if (!requestFailed && request instanceof Ci.nsIHttpChannel) {
      requestFailed = !request.requestSucceeded;
    }

    if (requestFailed || this._countRead == 0) {
      SearchUtils.log("loadListener: request failed!");
      // send null so the callback can deal with the failure
      this._bytes = null;
    }
    this._callback(this._bytes, this._engine);
    this._channel = null;
    this._engine = null;
  },

  // nsIStreamListener
  onDataAvailable(request, inputStream, offset, count) {
    this._stream.setInputStream(inputStream);

    // Get a byte array of the data
    this._bytes = this._bytes.concat(this._stream.readByteArray(count));
    this._countRead += count;
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    this._channel = newChannel;
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsIProgressEventSink
  onProgress(request, progress, progressMax) {},
  onStatus(request, status, statusArg) {},
};

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

function getVerificationHash(name) {
  let disclaimer =
    "By modifying this file, I agree that I am doing so " +
    "only within $appName itself, using official, user-driven search " +
    "engine selection processes, and in a way which does not circumvent " +
    "user consent. I acknowledge that any attempt to change this file " +
    "from outside of $appName is a malicious act, and will be responded " +
    "to accordingly.";

  let salt =
    OS.Path.basename(OS.Constants.Path.profileDir) +
    name +
    disclaimer.replace(/\$appName/g, Services.appinfo.name);

  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";

  // Data is an array of bytes.
  let data = converter.convertToByteArray(salt, {});
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  return hasher.finish(true);
}

/**
 * Gets a directory from the directory service.
 * @param {string} key
 *   The directory service key indicating the directory to get.
 * @param {nsIIDRef} iface
 *   The expected interface type of the directory information.
 * @returns {object}
 */
function getDir(key, iface) {
  if (!key) {
    SearchUtils.fail("getDir requires a directory key!");
  }

  return Services.dirsvc.get(key, iface || Ci.nsIFile);
}

/**
 * Sanitizes a name so that it can be used as a filename. If it cannot be
 * sanitized (e.g. no valid characters), then it returns a random name.
 *
 * @param {string} name
 *  The name to be sanitized.
 * @returns {string}
 *  The sanitized name.
 */
function sanitizeName(name) {
  const maxLength = 60;
  const minLength = 1;
  var result = name.toLowerCase();
  result = result.replace(/\s+/g, "-");
  result = result.replace(/[^-a-z0-9]/g, "");

  // Use a random name if our input had no valid characters.
  if (result.length < minLength) {
    result = Math.random()
      .toString(36)
      .replace(/^.*\./, "");
  }

  // Force max length.
  return result.substring(0, maxLength);
}

/**
 * A simple class to handle caching of preferences that may be read from
 * parameters.
 */
const ParamPreferenceCache = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
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
      SearchUtils.fail("missing name or value for QueryParameter!");
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
      // {moz:locale} and {moz:distributionID} are common
      if (name == MOZ_PARAM_LOCALE) {
        return Services.locale.requestedLocale;
      }
      if (name == MOZ_PARAM_DIST_ID) {
        return Services.prefs.getCharPref(
          SearchUtils.BROWSER_SEARCH_PREF + "distributionID",
          Services.appinfo.distributionID || ""
        );
      }
      // {moz:official} seems to have little use.
      if (name == MOZ_PARAM_OFFICIAL) {
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
  ["twitter", ["@twitter"]],
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
 * Creates an engineURL object, which holds the query URL and all parameters.
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
function EngineURL(mimeType, requestMethod, template, resultDomain) {
  if (!mimeType || !requestMethod || !template) {
    SearchUtils.fail("missing mimeType, method or template for EngineURL!");
  }

  var method = requestMethod.toUpperCase();
  var type = mimeType.toLowerCase();

  if (method != "GET" && method != "POST") {
    SearchUtils.fail('method passed to EngineURL must be "GET" or "POST"');
  }

  this.type = type;
  this.method = method;
  this.params = [];
  this.rels = [];

  var templateURI = SearchUtils.makeURI(template);
  if (!templateURI) {
    SearchUtils.fail(
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
      SearchUtils.fail(
        "new EngineURL: template uses invalid scheme!",
        Cr.NS_ERROR_FAILURE
      );
  }

  this.templateHost = templateURI.host;
  // If no resultDomain was specified in the engine definition file, use the
  // host from the template.
  this.resultDomain = resultDomain || this.templateHost;
}
EngineURL.prototype = {
  addParam(name, value, purpose) {
    this.params.push(new QueryParameter(name, value, purpose));
  },

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
  },

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

      const paramValue = param.value;
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
  },

  _getTermsParameterName() {
    let queryParam = this.params.find(p => p.value == "{" + USER_DEFINED + "}");
    return queryParam ? queryParam.name : "";
  },

  _hasRelation(rel) {
    return this.rels.some(e => e == rel.toLowerCase());
  },

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
  },

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
  },
};

/**
 * nsISearchEngine constructor.
 *
 * @param {object} options
 *   The options for this search engine. At least one of options.name,
 *   options.fileURI or options.uri are required.
 * @param {string} [options.name]
 *   The name to base the short name of the engine on. This is typically the
 *   display name where a pre-defined/sanitized short name is not available.
 * @param {string} [options.shortName]
 *   The short name to use for the engine. This should be known to match
 *   the basic requirements in sanitizeName for a short name.
 * @param {nsIFile} [options.fileURI]
 *   The file URI that points to the search engine data.
 * @param {nsIURI|string} [options.uri]
 *   Represents the location of the search engine data file.
 * @param {boolean} options.isBuiltin
 *   Indicates whether the engine is a app-provided or not. If it is, it will
 *   be treated as read-only.
 */
function SearchEngine(options = {}) {
  if (!("isBuiltin" in options)) {
    throw new Error("isBuiltin missing from options.");
  }
  this._isBuiltin = options.isBuiltin;
  this._urls = [];
  this._metaData = {};

  let file, uri;
  if ("name" in options) {
    this._shortName = sanitizeName(options.name);
  } else if ("shortName" in options) {
    this._shortName = options.shortName;
  } else if ("fileURI" in options && options.fileURI instanceof Ci.nsIFile) {
    file = options.fileURI;
  } else if ("uri" in options) {
    let optionsURI = options.uri;
    if (typeof optionsURI == "string") {
      optionsURI = SearchUtils.makeURI(optionsURI);
    }
    // makeURI can return null if the URI is invalid.
    if (!optionsURI || !(optionsURI instanceof Ci.nsIURI)) {
      throw new Components.Exception(
        "options.uri isn't a string nor an nsIURI",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    switch (optionsURI.scheme) {
      case "https":
      case "http":
      case "ftp":
      case "data":
      case "file":
      case "resource":
      case "chrome":
        uri = optionsURI;
        break;
      default:
        throw Components.Exception(
          "Invalid URI passed to SearchEngine constructor",
          Cr.NS_ERROR_INVALID_ARG
        );
    }
  } else {
    throw Components.Exception(
      "Invalid name/fileURI/uri options passed to SearchEngine",
      Cr.NS_ERROR_INVALID_ARG
    );
  }

  if (!this._shortName) {
    // If we don't have a shortName at this point, it's the first time we load
    // this engine, so let's generate the shortName, id and loadPath values.
    let shortName;
    if (file) {
      shortName = file.leafName;
    } else if (uri && uri instanceof Ci.nsIURL) {
      if (
        this._isBuiltin ||
        (gEnvironment.get("XPCSHELL_TEST_PROFILE_DIR") &&
          uri.scheme == "resource")
      ) {
        shortName = uri.fileName;
      }
    }
    if (shortName && shortName.endsWith(".xml")) {
      this._shortName = shortName.slice(0, -4);
    }
    this._loadPath = this.getAnonymizedLoadPath(file, uri);
  }
}

SearchEngine.prototype = {
  // Data set by the user.
  _metaData: null,
  // The data describing the engine, in the form of an XML document element.
  _data: null,
  // Anonymized path of where we initially loaded the engine from.
  // This will stay null for engines installed in the profile before we moved
  // to a JSON storage.
  _loadPath: null,
  // The engine's description
  _description: "",
  // Used to store the engine to replace, if we're an update to an existing
  // engine.
  _engineToUpdate: null,
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon: null,
  // The engine's name.
  _name: null,
  // The name of the charset used to submit the search terms.
  _queryCharset: null,
  // The engine's raw SearchForm value (URL string pointing to a search form).
  __searchForm: null,
  get _searchForm() {
    return this.__searchForm;
  },
  set _searchForm(value) {
    if (/^https?:/i.test(value)) {
      this.__searchForm = value;
    } else {
      SearchUtils.log(
        "_searchForm: Invalid URL dropped for " + this._name ||
          "the current engine"
      );
    }
  },
  // Whether to obtain user confirmation before adding the engine. This is only
  // used when the engine is first added to the list.
  _confirm: false,
  // Whether to set this as the current engine as soon as it is loaded.  This
  // is only used when the engine is first added to the list.
  _useNow: false,
  // A function to be invoked when this engine object's addition completes (or
  // fails). Only used for installation via addEngine.
  _installCallback: null,
  // The number of days between update checks for new versions
  _updateInterval: null,
  // The url to check at for a new update
  _updateURL: null,
  // The url to check for a new icon
  _iconUpdateURL: null,
  // The extension ID if added by an extension.
  _extensionID: null,
  // The locale, or "DEFAULT", if required.
  _locale: null,
  // Whether the engine is provided by the application.
  _isBuiltin: false,
  // The order hint from the configuration (if any).
  _orderHint: null,
  // The telemetry id from the configuration (if any).
  _telemetryId: null,

  /**
   * Retrieves the data from the engine's file asynchronously.
   * The document element is placed in the engine's data field.
   *
   * @param {nsIFile} file
   *   The file to load the search plugin from.
   */
  async _initFromFile(file) {
    if (!file || !(await OS.File.exists(file.path))) {
      SearchUtils.fail(
        "File must exist before calling initFromFile!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    let fileURI = Services.io.newFileURI(file);
    await this._retrieveSearchXMLData(fileURI.spec);

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the engine data from a URI. Initializes the engine, flushes to
   * disk, and notifies the search service once initialization is complete.
   *
   * @param {string|nsIURI} uri The uri to load the search plugin from.
   */
  _initFromURIAndLoad(uri) {
    let loadURI = uri instanceof Ci.nsIURI ? uri : SearchUtils.makeURI(uri);
    ENSURE_WARN(
      loadURI,
      "Must have URI when calling _initFromURIAndLoad!",
      Cr.NS_ERROR_UNEXPECTED
    );

    SearchUtils.log(
      '_initFromURIAndLoad: Downloading engine from: "' + loadURI.spec + '".'
    );

    var chan = SearchUtils.makeChannel(loadURI);

    if (this._engineToUpdate && chan instanceof Ci.nsIHttpChannel) {
      var lastModified = this._engineToUpdate.getAttr("updatelastmodified");
      if (lastModified) {
        chan.setRequestHeader("If-Modified-Since", lastModified, false);
      }
    }
    this._uri = loadURI;
    var listener = new loadListener(chan, this, this._onLoad);
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener);
  },

  /**
   * Retrieves the engine data from a URI asynchronously and initializes it.
   *
   * @param {nsIURI} uri
   *   The uri to load the search plugin from.
   */
  async _initFromURI(uri) {
    SearchUtils.log('_initFromURI: Loading engine from: "' + uri.spec + '".');
    await this._retrieveSearchXMLData(uri.spec);
    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the engine data for a given URI asynchronously.
   *
   * @param {string} url
   *   The URL to get engine data from.
   * @returns {Promise}
   *   A promise, resolved successfully if retrieveing data succeeds.
   */
  _retrieveSearchXMLData(url) {
    return new Promise(resolve => {
      let request = new XMLHttpRequest();
      request.overrideMimeType("text/xml");
      request.onload = event => {
        let responseXML = event.target.responseXML;
        this._data = responseXML.documentElement;
        resolve();
      };
      request.onerror = function(event) {
        resolve();
      };
      request.open("GET", url, true);
      request.send();
    });
  },

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
  },

  _confirmAddEngine() {
    var stringBundle = Services.strings.createBundle(SEARCH_BUNDLE);
    var titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");

    // Display only the hostname portion of the URL.
    var dialogMessage = stringBundle.formatStringFromName(
      "addEngineConfirmation",
      [this._name, this._uri.host]
    );
    var checkboxMessage = null;
    if (
      !Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "noCurrentEngine",
        false
      )
    ) {
      checkboxMessage = stringBundle.GetStringFromName(
        "addEngineAsCurrentText"
      );
    }

    var addButtonLabel = stringBundle.GetStringFromName(
      "addEngineAddButtonLabel"
    );

    var ps = Services.prompt;
    var buttonFlags =
      ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
      ps.BUTTON_TITLE_CANCEL * ps.BUTTON_POS_1 +
      ps.BUTTON_POS_0_DEFAULT;

    var checked = { value: false };
    // confirmEx returns the index of the button that was pressed.  Since "Add"
    // is button 0, we want to return the negation of that value.
    var confirm = !ps.confirmEx(
      null,
      titleMessage,
      dialogMessage,
      buttonFlags,
      addButtonLabel,
      null,
      null, // button 1 & 2 names not used
      checkboxMessage,
      checked
    );

    return { confirmed: confirm, useNow: checked.value };
  },

  /**
   * Handle the successful download of an engine. Initializes the engine and
   * triggers parsing of the data. The engine is then flushed to disk. Notifies
   * the search service once initialization is complete.
   *
   * @param {array} bytes
   *  The loaded search engine data.
   * @param {nsISearchEngine} engine
   *  The engine being loaded.
   */
  _onLoad(bytes, engine) {
    /**
     * Handle an error during the load of an engine by notifying the engine's
     * error callback, if any.
     *
     * @param {number} [errorCode]
     *   The relevant error code.
     */
    function onError(errorCode = Ci.nsISearchService.ERROR_UNKNOWN_FAILURE) {
      // Notify the callback of the failure
      if (engine._installCallback) {
        engine._installCallback(errorCode);
      }
    }

    function promptError(strings = {}, error = undefined) {
      onError(error);

      if (engine._engineToUpdate) {
        // We're in an update, so just fail quietly
        SearchUtils.log("updating " + engine._engineToUpdate.name + " failed");
        return;
      }
      var brandBundle = Services.strings.createBundle(BRAND_BUNDLE);
      var brandName = brandBundle.GetStringFromName("brandShortName");

      var searchBundle = Services.strings.createBundle(SEARCH_BUNDLE);
      var msgStringName = strings.error || "error_loading_engine_msg2";
      var titleStringName = strings.title || "error_loading_engine_title";
      var title = searchBundle.GetStringFromName(titleStringName);
      var text = searchBundle.formatStringFromName(msgStringName, [
        brandName,
        engine._location,
      ]);

      Services.ww.getNewPrompter(null).alert(title, text);
    }

    if (!bytes) {
      promptError();
      return;
    }

    var parser = new DOMParser();
    var doc = parser.parseFromBuffer(bytes, "text/xml");
    engine._data = doc.documentElement;

    try {
      // Initialize the engine from the obtained data
      engine._initFromData();
    } catch (ex) {
      SearchUtils.log("_onLoad: Failed to init engine!\n" + ex);
      // Report an error to the user
      if (ex.result == Cr.NS_ERROR_FILE_CORRUPTED) {
        promptError({
          error: "error_invalid_engine_msg2",
          title: "error_invalid_format_title",
        });
      } else {
        promptError();
      }
      return;
    }

    if (engine._engineToUpdate) {
      let engineToUpdate = engine._engineToUpdate.wrappedJSObject;

      // Make this new engine use the old engine's shortName, and preserve
      // metadata.
      engine._shortName = engineToUpdate._shortName;
      Object.keys(engineToUpdate._metaData).forEach(key => {
        engine.setAttr(key, engineToUpdate.getAttr(key));
      });
      engine._loadPath = engineToUpdate._loadPath;

      // Keep track of the last modified date, so that we can make conditional
      // requests for future updates.
      engine.setAttr("updatelastmodified", new Date().toUTCString());

      // Set the new engine's icon, if it doesn't yet have one.
      if (!engine._iconURI && engineToUpdate._iconURI) {
        engine._iconURI = engineToUpdate._iconURI;
      }
    } else {
      // Check that when adding a new engine (e.g., not updating an
      // existing one), a duplicate engine does not already exist.
      if (Services.search.getEngineByName(engine.name)) {
        // If we're confirming the engine load, then display a "this is a
        // duplicate engine" prompt; otherwise, fail silently.
        if (engine._confirm) {
          promptError(
            {
              error: "error_duplicate_engine_msg",
              title: "error_invalid_engine_title",
            },
            Ci.nsISearchService.ERROR_DUPLICATE_ENGINE
          );
        } else {
          onError(Ci.nsISearchService.ERROR_DUPLICATE_ENGINE);
        }
        SearchUtils.log("_onLoad: duplicate engine found, bailing");
        return;
      }

      // If requested, confirm the addition now that we have the title.
      // This property is only ever true for engines added via
      // nsISearchService::addEngine.
      if (engine._confirm) {
        var confirmation = engine._confirmAddEngine();
        SearchUtils.log(
          "_onLoad: confirm is " +
            confirmation.confirmed +
            "; useNow is " +
            confirmation.useNow
        );
        if (!confirmation.confirmed) {
          onError();
          return;
        }
        engine._useNow = confirmation.useNow;
      }

      engine._shortName = sanitizeName(engine.name);
      engine._loadPath = engine.getAnonymizedLoadPath(null, engine._uri);
      if (engine._extensionID) {
        engine._loadPath += ":" + engine._extensionID;
      }
      engine.setAttr("loadPathHash", getVerificationHash(engine._loadPath));
    }

    // Notify the search service of the successful load. It will deal with
    // updates by checking aEngine._engineToUpdate.
    SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.LOADED);

    // Notify the callback if needed
    if (engine._installCallback) {
      engine._installCallback();
    }
  },

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
  },

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
  },

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

    SearchUtils.log(
      '_setIcon: Setting icon url "' +
        limitURILength(uri.spec) +
        '" for engine "' +
        this.name +
        '".'
    );
    // Only accept remote icons from http[s] or ftp
    switch (uri.scheme) {
      // Fall through to the data case
      case "moz-extension":
      case "data":
        if (!this._hasPreferredIcon || isPreferred) {
          this._iconURI = uri;
          SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
          this._hasPreferredIcon = isPreferred;
        }

        if (width && height) {
          this._addIconToMap(width, height, iconURL);
        }
        break;
      case "http":
      case "https":
      case "ftp":
        SearchUtils.log(
          '_setIcon: Downloading icon: "' +
            uri.spec +
            '" for engine: "' +
            this.name +
            '"'
        );
        var chan = SearchUtils.makeChannel(uri);

        let iconLoadCallback = function(byteArray, engine) {
          // This callback may run after we've already set a preferred icon,
          // so check again.
          if (engine._hasPreferredIcon && !isPreferred) {
            return;
          }

          if (!byteArray) {
            SearchUtils.log("iconLoadCallback: load failed");
            return;
          }

          let contentType = chan.contentType;
          if (byteArray.length > SearchUtils.MAX_ICON_SIZE) {
            try {
              SearchUtils.log("iconLoadCallback: rescaling icon");
              [byteArray, contentType] = rescaleIcon(byteArray, contentType);
            } catch (ex) {
              SearchUtils.log("iconLoadCallback: got exception: " + ex);
              Cu.reportError(
                "Unable to set an icon for the search engine because: " + ex
              );
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

          SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.CHANGED);
          engine._hasPreferredIcon = isPreferred;
        };

        // If we're currently acting as an "update engine", then the callback
        // should set the icon on the engine we're updating and not us, since
        // |this| might be gone by the time the callback runs.
        var engineToSet = this._engineToUpdate || this;

        var listener = new loadListener(chan, engineToSet, iconLoadCallback);
        chan.notificationCallbacks = listener;
        chan.asyncOpen(listener);
        break;
    }
  },

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
      SearchUtils.log("_init: Initing search plugin from " + this._location);

      this._parse();
    } else {
      Cu.reportError("Invalid search plugin due to namespace not matching.");
      SearchUtils.fail(
        this._location + " is not a valid search plugin.",
        Cr.NS_ERROR_FILE_CORRUPTED
      );
    }
    // No need to keep a ref to our data (which in some cases can be a document
    // element) past this point
    this._data = null;
  },

  /**
   * Initialize an EngineURL object from metadata.
   *
   * @param {string} type
   *   The url type.
   * @param {object} params
   *   The URL parameters.
   * @param {string} [params.getParams]
   *   Any parameters for a GET method.
   * @param {string} [params.method]
   *   The type of method, defaults to GET.
   * @param {string} [params.mozParams]
   *   Any special Mozilla Parameters.
   * @param {string} [params.postParams]
   *   Any parameters for a POST method.
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
      let queries = new URLSearchParams(params.postParams);
      for (let [name, value] of queries) {
        url.addParam(name, value);
      }
    }

    if (params.getParams) {
      let queries = new URLSearchParams(params.getParams);
      for (let [name, value] of queries) {
        url.addParam(name, value);
      }
    }

    this._urls.push(url);
  },

  /**
   * Initialize this Engine object from a collection of metadata.
   *
   * @param {string} engineName
   *   The name of the search engine.
   * @param {object} params
   *   The URL parameters.
   * @param {string} [params.getParams]
   *   Any parameters for a GET method.
   * @param {string} [params.method]
   *   The type of method, defaults to GET.
   * @param {string} [params.mozParams]
   *   Any special Mozilla Parameters.
   * @param {string} [params.postParams]
   *   Any parameters for a POST method.
   * @param {string} params.template
   *   The url template.
   */
  _initFromMetadata(engineName, params) {
    this._extensionID = params.extensionID;
    this._locale = params.locale;
    this._orderHint = params.orderHint;
    this._telemetryId = params.telemetryId;

    this._initEngineURLFromMetaData(SearchUtils.URL_TYPE.SEARCH, {
      method: (params.searchPostParams && "POST") || params.method || "GET",
      template: params.template,
      getParams: params.searchGetParams,
      postParams: params.searchPostParams,
      mozParams: params.mozParams,
    });

    if (params.suggestURL) {
      this._initEngineURLFromMetaData(SearchUtils.URL_TYPE.SUGGEST_JSON, {
        method: (params.suggestPostParams && "POST") || params.method || "GET",
        template: params.suggestURL,
        getParams: params.suggestGetParams,
        postParams: params.suggestPostParams,
      });
    }

    if (params.queryCharset) {
      this._queryCharset = params.queryCharset;
    }
    if (params.postData) {
      let queries = new URLSearchParams(params.postData);
      for (let [name, value] of queries) {
        this.addParam(name, value);
      }
    }

    this._name = engineName;
    if (params.shortName) {
      this._shortName = params.shortName;
    }
    this.alias = params.alias;
    this._description = params.description;
    this.__searchForm = params.searchForm;
    if (params.iconURL) {
      this._setIcon(params.iconURL, true);
    }
    // Other sizes
    if (params.icons) {
      for (let icon of params.icons) {
        this._addIconToMap(icon.size, icon.size, icon.url);
      }
    }
  },

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
    var resultDomain = element.getAttribute("resultdomain");

    let rels = [];
    if (element.hasAttribute("rel")) {
      rels = element
        .getAttribute("rel")
        .toLowerCase()
        .split(/\s+/);
    }

    // Support an alternate suggestion type, see bug 1425827 for details.
    if (type == "application/json" && rels.includes("suggestions")) {
      type = SearchUtils.URL_TYPE.SUGGEST_JSON;
    }

    try {
      var url = new EngineURL(type, method, template, resultDomain);
    } catch (ex) {
      SearchUtils.fail(
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
          SearchUtils.log("_parseURL: Url element has an invalid param");
        }
      } else if (
        param.localName == "MozParam" &&
        // We only support MozParams for default search engines
        this.isAppProvided
      ) {
        let condition = param.getAttribute("condition");

        // MozParams must have a condition to be valid
        if (!condition) {
          let engineLoc = this._location;
          let paramName = param.getAttribute("name");
          SearchUtils.log(
            "_parseURL: MozParam (" +
              paramName +
              ") without a condition attribute found parsing engine: " +
              engineLoc
          );
          continue;
        }

        // We can't make these both use _addMozParam due to the fallback
        // handling - WebExtension parameters get treated as MozParams even
        // if they are not, and hence don't have the condition parameter, so
        // we can't warn for them.
        switch (condition) {
          case "purpose":
            url.addParam(
              param.getAttribute("name"),
              param.getAttribute("value"),
              param.getAttribute("purpose")
            );
            break;
          case "pref":
            url._addMozParam({
              pref: param.getAttribute("pref"),
              name: param.getAttribute("name"),
              condition: "pref",
            });
            break;
          default:
            let engineLoc = this._location;
            let paramName = param.getAttribute("name");
            SearchUtils.log(
              "_parseURL: MozParam (" +
                paramName +
                ") has an unknown condition: " +
                condition +
                ". Found parsing engine: " +
                engineLoc
            );
            break;
        }
      }
    }

    this._urls.push(url);
  },

  /**
   * Get the icon from an OpenSearch Image element.
   *
   * @param {HTMLLinkElement} element
   *   The OpenSearch URL element.
   * @see http://opensearch.a9.com/spec/1.1/description/#image
   */
  _parseImage(element) {
    SearchUtils.log(
      '_parseImage: Image textContent: "' +
        limitURILength(element.textContent) +
        '"'
    );

    let width = parseInt(element.getAttribute("width"), 10);
    let height = parseInt(element.getAttribute("height"), 10);
    let isPrefered = width == 16 && height == 16;

    if (isNaN(width) || isNaN(height) || width <= 0 || height <= 0) {
      SearchUtils.log(
        "OpenSearch image element must have positive width and height."
      );
      return;
    }

    this._setIcon(element.textContent, isPrefered, width, height);
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parse() {
    var doc = this._data;

    // The OpenSearch spec sets a default value for the input encoding.
    this._queryCharset = OS_PARAM_INPUT_ENCODING_DEF;

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
            SearchUtils.log("_parse: failed to parse URL child: " + ex);
          }
          break;
        case "Image":
          this._parseImage(child);
          break;
        case "InputEncoding":
          this._queryCharset = child.textContent.toUpperCase();
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
      SearchUtils.fail("_parse: No name, or missing URL!", Cr.NS_ERROR_FAILURE);
    }
    if (!this.supportsResponseType(SearchUtils.URL_TYPE.SEARCH)) {
      SearchUtils.fail(
        "_parse: No text/html result type!",
        Cr.NS_ERROR_FAILURE
      );
    }
  },

  /**
   * Init from a JSON record.
   *
   * @param {object} json
   *   The json record to use.
   */
  _initWithJSON(json) {
    this._name = json._name;
    this._shortName = json._shortName;
    this._loadPath = json._loadPath;
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
    if (json.filePath) {
      this._filePath = json.filePath;
    }
    if (json.extensionID) {
      this._extensionID = json.extensionID;
    }
    if (json.extensionLocale) {
      this._locale = json.extensionLocale;
    }
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
  },

  /**
   * Creates a JavaScript object that represents this engine.
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    var json = {
      _name: this._name,
      _shortName: this._shortName,
      _loadPath: this._loadPath,
      description: this.description,
      __searchForm: this.__searchForm,
      _iconURL: this._iconURL,
      _iconMapObj: this._iconMapObj,
      _metaData: this._metaData,
      _urls: this._urls,
      _isBuiltin: this._isBuiltin,
      _orderHint: this._orderHint,
      _telemetryId: this._telemetryId,
    };

    if (this._updateInterval) {
      json._updateInterval = this._updateInterval;
    }
    if (this._updateURL) {
      json._updateURL = this._updateURL;
    }
    if (this._iconUpdateURL) {
      json._iconUpdateURL = this._iconUpdateURL;
    }
    if (!this._hasPreferredIcon) {
      json._hasPreferredIcon = this._hasPreferredIcon;
    }
    if (this.queryCharset != SearchUtils.DEFAULT_QUERY_CHARSET) {
      json.queryCharset = this.queryCharset;
    }
    if (this._filePath) {
      // File path is stored so that we can remove legacy xml files
      // from the profile if the user removes the engine.
      json.filePath = this._filePath;
    }
    if (this._extensionID) {
      json.extensionID = this._extensionID;
    }
    if (this._locale) {
      json.extensionLocale = this._locale;
    }

    return json;
  },

  setAttr(name, val) {
    this._metaData[name] = val;
  },

  getAttr(name) {
    return this._metaData[name] || undefined;
  },

  // nsISearchEngine
  get alias() {
    return this.getAttr("alias");
  },
  set alias(val) {
    var value = val ? val.trim() : null;
    this.setAttr("alias", value);
    SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
  },

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
    return this._telemetryId || this.identifier || `other-${this.name}`;
  },

  /**
   * Return the built-in identifier of app-provided engines.
   *
   * @returns {string|null}
   *   Returns a valid if this is a built-in engine, null otherwise.
   */
  get identifier() {
    // No identifier if If the engine isn't app-provided
    return this.isAppProvided ? this._shortName : null;
  },

  get description() {
    return this._description;
  },

  get hidden() {
    return this.getAttr("hidden") || false;
  },
  set hidden(val) {
    var value = !!val;
    if (value != this.hidden) {
      this.setAttr("hidden", value);
      SearchUtils.notifyAction(this, SearchUtils.MODIFIED_TYPE.CHANGED);
    }
  },

  get iconURI() {
    if (this._iconURI) {
      return this._iconURI;
    }
    return null;
  },

  get _iconURL() {
    if (!this._iconURI) {
      return "";
    }
    return this._iconURI.spec;
  },

  // Where the engine is being loaded from: will return the URI's spec if the
  // engine is being downloaded and does not yet have a file. This is only used
  // for logging and error messages.
  get _location() {
    if (this._uri) {
      return this._uri.spec;
    }

    return this._loadPath;
  },

  // This indicates where we found the .xml file to load the engine,
  // and attempts to hide user-identifiable data (such as username).
  getAnonymizedLoadPath(file, uri) {
    /* Examples of expected output:
     *   jar:[app]/omni.ja!browser/engine.xml
     *     'browser' here is the name of the chrome package, not a folder.
     *   [profile]/searchplugins/engine.xml
     *   [distribution]/searchplugins/common/engine.xml
     *   [other]/engine.xml
     */

    const NS_XPCOM_CURRENT_PROCESS_DIR = "XCurProcD";
    const NS_APP_USER_PROFILE_50_DIR = "ProfD";
    const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

    const knownDirs = {
      app: NS_XPCOM_CURRENT_PROCESS_DIR,
      profile: NS_APP_USER_PROFILE_50_DIR,
      distribution: XRE_APP_DISTRIBUTION_DIR,
    };

    let leafName = this._shortName;
    if (!leafName) {
      return "null";
    }
    leafName += ".xml";

    let prefix = "",
      suffix = "";
    if (!file) {
      if (uri.schemeIs("resource")) {
        uri = SearchUtils.makeURI(
          Services.io
            .getProtocolHandler("resource")
            .QueryInterface(Ci.nsISubstitutingProtocolHandler)
            .resolveURI(uri)
        );
      }
      let scheme = uri.scheme;
      let packageName = "";
      if (scheme == "chrome") {
        packageName = uri.hostPort;
        uri = gChromeReg.convertChromeURL(uri);
      }

      if (AppConstants.platform == "android") {
        // On Android the omni.ja file isn't at the same path as the binary
        // used to start the process. We tweak the path here so that the code
        // shared with Desktop will correctly identify files from the omni.ja
        // file as coming from the [app] folder.
        let appPath = Services.io
          .getProtocolHandler("resource")
          .QueryInterface(Ci.nsIResProtocolHandler)
          .getSubstitution("android");
        if (appPath) {
          appPath = appPath.spec;
          let spec = uri.spec;
          if (spec.includes(appPath)) {
            let appURI = Services.io.newFileURI(getDir(knownDirs.app));
            uri = Services.io.newURI(spec.replace(appPath, appURI.spec));
          }
        }
      }

      if (uri instanceof Ci.nsINestedURI) {
        prefix = "jar:";
        suffix = "!" + packageName + "/" + leafName;
        uri = uri.innermostURI;
      }
      if (uri instanceof Ci.nsIFileURL) {
        file = uri.file;
      } else {
        let path = "[" + scheme + "]";
        if (/^(?:https?|ftp)$/.test(scheme)) {
          path += uri.host;
        }
        return path + "/" + leafName;
      }
    }

    let id;
    let enginePath = file.path;

    for (let key in knownDirs) {
      let path;
      try {
        path = getDir(knownDirs[key]).path;
      } catch (e) {
        // Getting XRE_APP_DISTRIBUTION_DIR throws during unit tests.
        continue;
      }
      if (enginePath.startsWith(path)) {
        id =
          "[" + key + "]" + enginePath.slice(path.length).replace(/\\/g, "/");
        break;
      }
    }

    // If the folder doesn't have a known ancestor, don't record its path to
    // avoid leaking user identifiable data.
    if (!id) {
      id = "[other]/" + file.leafName;
    }

    return prefix + id + suffix;
  },

  get _isDistribution() {
    return !!(
      this._extensionID &&
      Services.prefs.getCharPref(
        `extensions.installedDistroAddon.${this._extensionID}`,
        ""
      )
    );
  },

  get isAppProvided() {
    if (this._extensionID) {
      return this._isBuiltin || this._isDistribution;
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
  },

  get _hasUpdates() {
    // Whether or not the engine has an update URL
    let selfURL = this._getURLOfType(SearchUtils.URL_TYPE.OPENSEARCH, "self");
    return !!(this._updateURL || this._iconUpdateURL || selfURL);
  },

  get name() {
    return this._name;
  },

  get searchForm() {
    return this._getSearchFormWithPurpose();
  },

  /* Internal aliases for default engines only. */
  __internalAliases: null,
  get _internalAliases() {
    if (!this.__internalAliases) {
      this.__internalAliases = getInternalAliases(this);
    }
    return this.__internalAliases;
  },

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
      ENSURE_WARN(htmlUrl, "Engine has no HTML URL!", Cr.NS_ERROR_UNEXPECTED);
      this._searchForm = SearchUtils.makeURI(htmlUrl.template).prePath;
    }

    return ParamSubstitution(this._searchForm, "", this);
  },

  get queryCharset() {
    if (this._queryCharset) {
      return this._queryCharset;
    }
    return (this._queryCharset = "windows-1252"); // the default
  },

  // from nsISearchEngine
  addParam(name, value, responseType) {
    if (!name || value == null) {
      SearchUtils.fail("missing name or value for nsISearchEngine::addParam!");
    }
    ENSURE_WARN(
      !this._isBuiltin,
      "called nsISearchEngine::addParam on a built-in engine!",
      Cr.NS_ERROR_FAILURE
    );
    if (!responseType) {
      responseType = SearchUtils.URL_TYPE.SEARCH;
    }

    var url = this._getURLOfType(responseType);
    if (!url) {
      SearchUtils.fail(
        "Engine object has no URL for response type " + responseType,
        Cr.NS_ERROR_FAILURE
      );
    }

    url.addParam(name, value);
  },

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
  },

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

    SearchUtils.log(
      'getSubmission: In data: "' + data + '"; Purpose: "' + purpose + '"'
    );
    var submissionData = "";
    try {
      submissionData = Services.textToSubURI.ConvertAndEscape(
        this.queryCharset,
        data
      );
    } catch (ex) {
      SearchUtils.log("getSubmission: Falling back to default queryCharset!");
      submissionData = Services.textToSubURI.ConvertAndEscape(
        SearchUtils.DEFAULT_QUERY_CHARSET,
        data
      );
    }
    SearchUtils.log('getSubmission: Out data: "' + submissionData + '"');
    return url.getSubmission(submissionData, this, purpose);
  },

  // from nsISearchEngine
  supportsResponseType(type) {
    return this._getURLOfType(type) != null;
  },

  // from nsISearchEngine
  getResultDomain(responseType) {
    if (!responseType) {
      responseType =
        AppConstants.platform == "android"
          ? this._defaultMobileResponseType
          : SearchUtils.URL_TYPE.SEARCH;
    }

    SearchUtils.log('getResultDomain: responseType: "' + responseType + '"');

    let url = this._getURLOfType(responseType);
    if (url) {
      return url.resultDomain;
    }
    return "";
  },

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
  },

  // nsISupports
  QueryInterface: ChromeUtils.generateQI([Ci.nsISearchEngine]),

  get wrappedJSObject() {
    return this;
  },

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
  },

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
  },

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
      throw Cr.NS_ERROR_INVALID_ARG;
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
  },
};

// nsISearchSubmission
function Submission(uri, postData = null) {
  this._uri = uri;
  this._postData = postData;
}
Submission.prototype = {
  get uri() {
    return this._uri;
  },
  get postData() {
    return this._postData;
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsISearchSubmission]),
};

var EXPORTED_SYMBOLS = ["SearchEngine", "getVerificationHash"];
