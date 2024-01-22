/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * OpenSearchLoader is used for loading OpenSearch definitions from content.
 */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "OpenSearchLoader",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

// The namespaces from the specification at
// https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#namespace
const OPENSEARCH_NS_10 = "http://a9.com/-/spec/opensearch/1.0/";
const OPENSEARCH_NS_11 = "http://a9.com/-/spec/opensearch/1.1/";

// Although the specification at gives the namespace names defined above, many
// existing OpenSearch engines are using the following versions. We therefore
// allow any one of these.
const OPENSEARCH_NAMESPACES = [
  OPENSEARCH_NS_11,
  OPENSEARCH_NS_10,
  "http://a9.com/-/spec/opensearchdescription/1.1/",
  "http://a9.com/-/spec/opensearchdescription/1.0/",
];

// The name of the element defining the OpenSearch definition.
const OPENSEARCH_LOCALNAME = "OpenSearchDescription";

// These were OpenSearch definitions for engines used internally by Mozilla.
// It may be possible to deprecate/remove these in future.
const MOZSEARCH_NS_10 = "http://www.mozilla.org/2006/browser/search/";
const MOZSEARCH_LOCALNAME = "SearchPlugin";

/**
 * @typedef {object} OpenSearchProperties
 * @property {string} name
 *   The display name of the engine.
 * @property {nsIURI} installURL
 *   The URL that the engine was initially loaded from.
 * @property {string} [description]
 *   The description of the engine.
 * @property {string} [queryCharset]
 *   The character set to use for encoding query values.
 * @property {string} [searchForm]
 *   Non-standard. The search form URL.
 * @property {string} [UpdateUrl]
 *   Non-standard. The update URL for the engine.
 * @property {number} [UpdateInterval]
 *   Non-standard. The update interval for the engine.
 * @property {string} [IconUpdateUrl]
 *   Non-standard. The update URL for the icon.
 * @property {OpenSearchURL[]} urls
 *   An array of URLs associated with the engine.
 * @property {OpenSearchImage[]} images
 *   An array of images assocaiated with the engine.
 */

/**
 * @typedef {object} OpenSearchURL
 * @property {string} type
 *   The OpenSearch based type of the URL see SearchUtils.URL_TYPE.
 * @property {string} method
 *   The method of submission for the URL: GET or POST.
 * @property {string} template
 *   The template for the URL.
 * @property {object[]} params
 *   An array of additional properties of name/value pairs. These are not part
 *   of the OpenSearch specification, but were used in Firefox prior to Firefox 78.
 * @property {string[]} rels
 *   An array of strings that define the relationship of this URL.
 *
 * @see SearchUtils.URL_TYPE
 * @see https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#url-rel-values
 */

/**
 * @typedef {object} OpenSearchImage
 * @property {string} url
 *   The source URL of the image.
 * @property {boolean} isPrefered
 *   If this image is of the preferred 16x16 size.
 * @property {width} width
 *   The reported width of the image.
 * @property {height} height
 *   The reported height of the image.
 */

/**
 * Retrieves the engine data from a URI and returns it.
 *
 * @param {nsIURI} sourceURI
 *   The uri from which to load the OpenSearch engine data.
 * @param {string} [lastModified]
 *   The UTC date when the engine was last updated, if any.
 * @returns {OpenSearchProperties}
 *   The properties of the loaded OpenSearch engine.
 */
export async function loadAndParseOpenSearchEngine(sourceURI, lastModified) {
  if (!sourceURI) {
    throw Components.Exception(
      sourceURI,
      "Must have URI when calling _install!",
      Cr.NS_ERROR_UNEXPECTED
    );
  }
  if (!/^https?$/i.test(sourceURI.scheme)) {
    throw Components.Exception(
      "Invalid URI passed to SearchEngine constructor",
      Cr.NS_ERROR_INVALID_ARG
    );
  }

  lazy.logConsole.debug("Downloading OpenSearch engine from:", sourceURI.spec);

  let xmlData = await loadEngineXML(sourceURI, lastModified);
  let xmlDocument = await parseXML(xmlData);

  lazy.logConsole.debug("Loading search plugin");

  let engineData;
  try {
    engineData = processXMLDocument(xmlDocument);
  } catch (ex) {
    lazy.logConsole.error("parseData: Failed to init engine!", ex);

    if (ex.result == Cr.NS_ERROR_FILE_CORRUPTED) {
      throw Components.Exception(
        "",
        Ci.nsISearchService.ERROR_ENGINE_CORRUPTED
      );
    }
    throw Components.Exception("", Ci.nsISearchService.ERROR_DOWNLOAD_FAILURE);
  }

  engineData.installURL = sourceURI;
  return engineData;
}

/**
 * Loads the engine XML from the given URI.
 *
 * @param {nsIURI} sourceURI
 *   The uri from which to load the OpenSearch engine data.
 * @param {string} [lastModified]
 *   The UTC date when the engine was last updated, if any.
 * @returns {Promise}
 *   A promise that is resolved with the data if the engine is successfully loaded
 *   and rejected otherwise.
 */
function loadEngineXML(sourceURI, lastModified) {
  var chan = lazy.SearchUtils.makeChannel(
    sourceURI,
    // OpenSearchEngine is loading a definition file for a search engine,
    // TYPE_DOCUMENT captures that load best.
    Ci.nsIContentPolicy.TYPE_DOCUMENT
  );

  if (lastModified && chan instanceof Ci.nsIHttpChannel) {
    chan.setRequestHeader("If-Modified-Since", lastModified, false);
  }
  let loadPromise = Promise.withResolvers();

  let loadHandler = data => {
    if (!data) {
      loadPromise.reject(
        Components.Exception("", Ci.nsISearchService.ERROR_DOWNLOAD_FAILURE)
      );
      return;
    }
    loadPromise.resolve(data);
  };

  var listener = new lazy.SearchUtils.LoadListener(
    chan,
    /(^text\/|xml$)/,
    loadHandler
  );
  chan.notificationCallbacks = listener;
  chan.asyncOpen(listener);

  return loadPromise.promise;
}

/**
 * Parses an engines XML data into a document element.
 *
 * @param {number[]} xmlData
 *   The loaded search engine data.
 * @returns {Element}
 *   A document element containing the parsed data.
 */
function parseXML(xmlData) {
  var parser = new DOMParser();
  var doc = parser.parseFromBuffer(xmlData, "text/xml");

  if (!doc?.documentElement) {
    throw Components.Exception(
      "Could not parse file",
      Ci.nsISearchService.ERROR_ENGINE_CORRUPTED
    );
  }

  if (!hasExpectedNamspeace(doc.documentElement)) {
    throw Components.Exception(
      "Not a valid OpenSearch xml file",
      Ci.nsISearchService.ERROR_ENGINE_CORRUPTED
    );
  }
  return doc.documentElement;
}

/**
 * Extract search engine information from the given document into a form that
 * can be passed to an OpenSearchEngine.
 *
 * @param {Element} xmlDocument
 *   The document to examine.
 * @returns {OpenSearchProperties}
 *   The properties of the OpenSearch engine.
 */
function processXMLDocument(xmlDocument) {
  let result = { urls: [], images: [] };

  for (let i = 0; i < xmlDocument.children.length; ++i) {
    var child = xmlDocument.children[i];
    switch (child.localName) {
      case "ShortName":
        result.name = child.textContent;
        break;
      case "Description":
        result.description = child.textContent;
        break;
      case "Url":
        try {
          result.urls.push(parseURL(child));
        } catch (ex) {
          // Parsing of the element failed, just skip it.
          lazy.logConsole.error("Failed to parse URL child:", ex);
        }
        break;
      case "Image": {
        let imageData = parseImage(child);
        if (imageData) {
          result.images.push(imageData);
        }
        break;
      }
      case "InputEncoding":
        // If this is not specified we fallback to the SearchEngine constructor
        // which currently uses SearchUtils.DEFAULT_QUERY_CHARSET which is
        // UTF-8 - the same as for OpenSearch.
        result.queryCharset = child.textContent;
        break;

      // Non-OpenSearch elements
      case "SearchForm":
        result.searchForm = child.textContent;
        break;
      case "UpdateUrl":
        result.updateURL = child.textContent;
        break;
      case "UpdateInterval":
        result.updateInterval = parseInt(child.textContent);
        break;
      case "IconUpdateUrl":
        result.iconUpdateURL = child.textContent;
        break;
    }
  }
  if (!result.name || !result.urls.length) {
    throw Components.Exception(
      "_parse: No name, or missing URL!",
      Cr.NS_ERROR_FAILURE
    );
  }
  if (!result.urls.find(url => url.type == lazy.SearchUtils.URL_TYPE.SEARCH)) {
    throw Components.Exception(
      "_parse: No text/html result type!",
      Cr.NS_ERROR_FAILURE
    );
  }
  return result;
}

/**
 * Extracts data from an OpenSearch URL element and creates an object which can
 * be used to create an OpenSearchEngine's URL.
 *
 * @param {Element} element
 *   The OpenSearch URL element.
 * @returns {OpenSearchURL}
 *   The extracted URL data.
 * @throws NS_ERROR_FAILURE if a URL object could not be created.
 *
 * @see https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#the-url-element
 */
function parseURL(element) {
  var type = element.getAttribute("type");
  // According to the spec, method is optional, defaulting to "GET" if not
  // specified.
  var method = element.getAttribute("method") || "GET";
  var template = element.getAttribute("template");

  let rels = [];
  if (element.hasAttribute("rel")) {
    rels = element.getAttribute("rel").toLowerCase().split(/\s+/);
  }

  // Support an alternate suggestion type, see bug 1425827 for details.
  if (type == "application/json" && rels.includes("suggestions")) {
    type = lazy.SearchUtils.URL_TYPE.SUGGEST_JSON;
  }

  let url = {
    type,
    method,
    template,
    params: [],
    rels,
  };

  // Non-standard. Used to be for Mozilla search engine files.
  for (var i = 0; i < element.children.length; ++i) {
    var param = element.children[i];
    if (param.localName == "Param") {
      url.params.push({
        name: param.getAttribute("name"),
        value: param.getAttribute("value"),
      });
    }
  }

  return url;
}

/**
 * Extracts an icon from an OpenSearch Image element.
 *
 * @param {Element} element
 *   The OpenSearch URL element.
 * @returns {OpenSearchImage}
 *   The properties of the image.
 * @see https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#the-image-element
 */
function parseImage(element) {
  let width = parseInt(element.getAttribute("width"), 10);
  let height = parseInt(element.getAttribute("height"), 10);
  let isPrefered = width == 16 && height == 16;

  if (isNaN(width) || isNaN(height) || width <= 0 || height <= 0) {
    lazy.logConsole.warn(
      "OpenSearch image element must have positive width and height."
    );
    return null;
  }

  return {
    url: element.textContent,
    isPrefered,
    width,
    height,
  };
}

/**
 * Confirms if the document has the expected namespace.
 *
 * @param {DOMElement} element
 *   The document to check.
 * @returns {boolean}
 *   True if the document matches the namespace.
 */
function hasExpectedNamspeace(element) {
  return (
    (element.localName == MOZSEARCH_LOCALNAME &&
      element.namespaceURI == MOZSEARCH_NS_10) ||
    (element.localName == OPENSEARCH_LOCALNAME &&
      OPENSEARCH_NAMESPACES.includes(element.namespaceURI))
  );
}
