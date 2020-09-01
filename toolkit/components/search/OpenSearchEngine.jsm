/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  EngineURL: "resource://gre/modules/SearchEngine.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  gChromeReg: ["@mozilla.org/chrome/chrome-registry;1", "nsIChromeRegistry"],
  gEnvironment: ["@mozilla.org/process/environment;1", "nsIEnvironment"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gModernConfig",
  SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
  false
);

XPCOMUtils.defineLazyGetter(this, "logConsole", () => {
  return console.createInstance({
    prefix: "OpenSearchEngine",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

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

const OS_PARAM_INPUT_ENCODING_DEF = "UTF-8";

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
class OpenSearchEngine extends SearchEngine {
  // The data describing the engine, in the form of an XML document element.
  _data = null;
  // A function to be invoked when this engine object's addition completes (or
  // fails). Only used for installation via addEngine.
  _installCallback = null;

  /**
   * Constructor.
   *
   * @param {object} options
   *   The options for this search engine. At least one of
   *   options.fileURI or options.uri are required.
   * @param {nsIFile} [options.fileURI]
   *   The file URI that points to the search engine data.
   * @param {nsIURI|string} [options.uri]
   *   Represents the location of the search engine data file.
   */
  constructor(options = {}) {
    let file;
    let uri;
    let shortName;
    if ("fileURI" in options && options.fileURI instanceof Ci.nsIFile) {
      file = options.fileURI;
      shortName = file.leafName;
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
      if (
        gEnvironment.get("XPCSHELL_TEST_PROFILE_DIR") &&
        uri.scheme == "resource"
      ) {
        shortName = uri.fileName;
      }
    }

    if (shortName && shortName.endsWith(".xml")) {
      shortName = shortName.slice(0, -4);
    }

    super({
      // These engines are never app-provided in modern config.
      isAppProvided: gModernConfig ? false : options.isAppProvided,
      loadPath: OpenSearchEngine.getAnonymizedLoadPath(shortName, file, uri),
      shortName,
    });
  }

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

    logConsole.debug(
      "_initFromURIAndLoad: Downloading engine from:",
      loadURI.spec
    );

    var chan = SearchUtils.makeChannel(loadURI);

    if (this._engineToUpdate && chan instanceof Ci.nsIHttpChannel) {
      var lastModified = this._engineToUpdate.getAttr("updatelastmodified");
      if (lastModified) {
        chan.setRequestHeader("If-Modified-Since", lastModified, false);
      }
    }
    this._uri = loadURI;
    var listener = new SearchUtils.LoadListener(chan, this, this._onLoad);
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener);
  }

  /**
   * Retrieves the data from the engine's file asynchronously.
   * The document element is placed in the engine's data field.
   *
   * @param {nsIFile} file
   *   The file to load the search plugin from.
   */
  async _initFromFile(file) {
    if (!file || !(await OS.File.exists(file.path))) {
      throw Components.Exception(
        "File must exist before calling initFromFile!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    let fileURI = Services.io.newFileURI(file);
    await this._retrieveSearchXMLData(fileURI.spec);

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  }

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
  }

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
        logConsole.warn("Failed to update", engine._engineToUpdate.name);
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
      logConsole.error("_onLoad: Failed to init engine!", ex);
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
        onError(Ci.nsISearchService.ERROR_DUPLICATE_ENGINE);
        logConsole.debug("_onLoad: duplicate engine found, bailing");
        return;
      }

      engine._shortName = SearchUtils.sanitizeName(engine.name);
      engine._loadPath = OpenSearchEngine.getAnonymizedLoadPath(
        engine._shortName,
        null,
        engine._uri
      );
      if (engine._extensionID) {
        engine._loadPath += ":" + engine._extensionID;
      }
      engine.setAttr(
        "loadPathHash",
        SearchUtils.getVerificationHash(engine._loadPath)
      );
    }

    // Notify the search service of the successful load. It will deal with
    // updates by checking aEngine._engineToUpdate.
    SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.LOADED);

    // Notify the callback if needed
    if (engine._installCallback) {
      engine._installCallback();
    }
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
      logConsole.debug("Initing search plugin from", this._location);

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
          logConsole.error("_parseURL: Url element has an invalid param");
        }
      } else if (
        param.localName == "MozParam" &&
        // We only support MozParams for default search engines
        this.isAppProvided
      ) {
        let condition = param.getAttribute("condition");

        if (!condition) {
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
            // MozParams must have a condition to be valid
            logConsole.error(
              "Parsing engine:",
              this._location,
              "MozParam:",
              param.getAttribute("name"),
              "has an unknown condition:",
              condition
            );
            break;
        }
      }
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
      logConsole.warn(
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
            logConsole.error("Failed to parse URL child:", ex);
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
      throw Components.Exception(
        "_parse: No name, or missing URL!",
        Cr.NS_ERROR_FAILURE
      );
    }
    if (!this.supportsResponseType(SearchUtils.URL_TYPE.SEARCH)) {
      throw Components.Exception(
        "_parse: No text/html result type!",
        Cr.NS_ERROR_FAILURE
      );
    }
  }

  get _hasUpdates() {
    // Whether or not the engine has an update URL
    let selfURL = this._getURLOfType(SearchUtils.URL_TYPE.OPENSEARCH, "self");
    return !!(this._updateURL || this._iconUpdateURL || selfURL);
  }

  /**
   * Gets a directory from the directory service.
   * @param {string} key
   *   The directory service key indicating the directory to get.
   * @param {nsIIDRef} iface
   *   The expected interface type of the directory information.
   * @returns {object}
   */
  static getDir(key, iface) {
    return Services.dirsvc.get(key, iface || Ci.nsIFile);
  }

  // This indicates where we found the .xml file to load the engine,
  // and attempts to hide user-identifiable data (such as username).
  static getAnonymizedLoadPath(shortName, file, uri) {
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

    let leafName = shortName;
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
            let appURI = Services.io.newFileURI(
              OpenSearchEngine.getDir(knownDirs.app)
            );
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
        path = this.getDir(knownDirs[key]).path;
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
  }
}

var EXPORTED_SYMBOLS = ["OpenSearchEngine"];
