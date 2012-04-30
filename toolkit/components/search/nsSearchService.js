# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Browser Search Service.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com> (Original author)
#   Gavin Sharp <gavin@gavinsharp.com>
#   Joe Hughes  <joe@retrovirus.com>
#   Pamela Greene <pamg.bugs@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

// Directory service keys
const NS_APP_SEARCH_DIR_LIST  = "SrchPluginsDL";
const NS_APP_USER_SEARCH_DIR  = "UsrSrchPlugns";
const NS_APP_SEARCH_DIR       = "SrchPlugns";
const NS_APP_USER_PROFILE_50_DIR = "ProfD";

// Search engine "locations". If this list is changed, be sure to update
// the engine's _isDefault function accordingly.
const SEARCH_APP_DIR = 1;
const SEARCH_PROFILE_DIR = 2;
const SEARCH_IN_EXTENSION = 3;
const SEARCH_JAR = 4;

// See documentation in nsIBrowserSearchService.idl.
const SEARCH_ENGINE_TOPIC        = "browser-search-engine-modified";
const QUIT_APPLICATION_TOPIC     = "quit-application";

const SEARCH_ENGINE_REMOVED      = "engine-removed";
const SEARCH_ENGINE_ADDED        = "engine-added";
const SEARCH_ENGINE_CHANGED      = "engine-changed";
const SEARCH_ENGINE_LOADED       = "engine-loaded";
const SEARCH_ENGINE_CURRENT      = "engine-current";

// The following constants are left undocumented in nsIBrowserSearchService.idl
// For the moment, they are meant for testing/debugging purposes only.

/**
 * Topic used for events involving the service itself.
 */
const SEARCH_SERVICE_TOPIC       = "browser-search-service";

/**
 * Sent whenever metadata is fully written to disk.
 */
const SEARCH_SERVICE_METADATA_WRITTEN  = "write-metadata-to-disk-complete";

const SEARCH_TYPE_MOZSEARCH      = Ci.nsISearchEngine.TYPE_MOZSEARCH;
const SEARCH_TYPE_OPENSEARCH     = Ci.nsISearchEngine.TYPE_OPENSEARCH;
const SEARCH_TYPE_SHERLOCK       = Ci.nsISearchEngine.TYPE_SHERLOCK;

const SEARCH_DATA_XML            = Ci.nsISearchEngine.DATA_XML;
const SEARCH_DATA_TEXT           = Ci.nsISearchEngine.DATA_TEXT;

// File extensions for search plugin description files
const XML_FILE_EXT      = "xml";
const SHERLOCK_FILE_EXT = "src";

// Delay for lazy serialization (ms)
const LAZY_SERIALIZE_DELAY = 100;

// Delay for batching invalidation of the JSON cache (ms)
const CACHE_INVALIDATION_DELAY = 1000;

// Current cache version. This should be incremented if the format of the cache
// file is modified.
const CACHE_VERSION = 7;

const ICON_DATAURL_PREFIX = "data:image/x-icon;base64,";

// Supported extensions for Sherlock plugin icons
const SHERLOCK_ICON_EXTENSIONS = [".gif", ".png", ".jpg", ".jpeg"];

const NEW_LINES = /(\r\n|\r|\n)/;

// Set an arbitrary cap on the maximum icon size. Without this, large icons can
// cause big delays when loading them at startup.
const MAX_ICON_SIZE   = 10000;

// Default charset to use for sending search parameters. ISO-8859-1 is used to
// match previous nsInternetSearchService behavior.
const DEFAULT_QUERY_CHARSET = "ISO-8859-1";

const SEARCH_BUNDLE = "chrome://global/locale/search/search.properties";
const BRAND_BUNDLE = "chrome://branding/locale/brand.properties";

const OPENSEARCH_NS_10  = "http://a9.com/-/spec/opensearch/1.0/";
const OPENSEARCH_NS_11  = "http://a9.com/-/spec/opensearch/1.1/";

// Although the specification at http://opensearch.a9.com/spec/1.1/description/
// gives the namespace names defined above, many existing OpenSearch engines
// are using the following versions.  We therefore allow either.
const OPENSEARCH_NAMESPACES = [
  OPENSEARCH_NS_11, OPENSEARCH_NS_10,
  "http://a9.com/-/spec/opensearchdescription/1.1/",
  "http://a9.com/-/spec/opensearchdescription/1.0/"
];

const OPENSEARCH_LOCALNAME = "OpenSearchDescription";

const MOZSEARCH_NS_10     = "http://www.mozilla.org/2006/browser/search/";
const MOZSEARCH_LOCALNAME = "SearchPlugin";

const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
const URLTYPE_SEARCH_HTML  = "text/html";
const URLTYPE_OPENSEARCH   = "application/opensearchdescription+xml";

// Empty base document used to serialize engines to file.
const EMPTY_DOC = "<?xml version=\"1.0\"?>\n" +
                  "<" + MOZSEARCH_LOCALNAME +
                  " xmlns=\"" + MOZSEARCH_NS_10 + "\"" +
                  " xmlns:os=\"" + OPENSEARCH_NS_11 + "\"" +
                  "/>";

const BROWSER_SEARCH_PREF = "browser.search.";

const USER_DEFINED = "{searchTerms}";

// Custom search parameters
#ifdef MOZ_OFFICIAL_BRANDING
const MOZ_OFFICIAL = "official";
#else
const MOZ_OFFICIAL = "unofficial";
#endif
#expand const MOZ_DISTRIBUTION_ID = __MOZ_DISTRIBUTION_ID__;

const MOZ_PARAM_LOCALE         = /\{moz:locale\}/g;
const MOZ_PARAM_DIST_ID        = /\{moz:distributionID\}/g;
const MOZ_PARAM_OFFICIAL       = /\{moz:official\}/g;

// Supported OpenSearch parameters
// See http://opensearch.a9.com/spec/1.1/querysyntax/#core
const OS_PARAM_USER_DEFINED    = /\{searchTerms\??\}/g;
const OS_PARAM_INPUT_ENCODING  = /\{inputEncoding\??\}/g;
const OS_PARAM_LANGUAGE        = /\{language\??\}/g;
const OS_PARAM_OUTPUT_ENCODING = /\{outputEncoding\??\}/g;

// Default values
const OS_PARAM_LANGUAGE_DEF         = "*";
const OS_PARAM_OUTPUT_ENCODING_DEF  = "UTF-8";
const OS_PARAM_INPUT_ENCODING_DEF   = "UTF-8";

// "Unsupported" OpenSearch parameters. For example, we don't support
// page-based results, so if the engine requires that we send the "page index"
// parameter, we'll always send "1".
const OS_PARAM_COUNT        = /\{count\??\}/g;
const OS_PARAM_START_INDEX  = /\{startIndex\??\}/g;
const OS_PARAM_START_PAGE   = /\{startPage\??\}/g;

// Default values
const OS_PARAM_COUNT_DEF        = "20"; // 20 results
const OS_PARAM_START_INDEX_DEF  = "1";  // start at 1st result
const OS_PARAM_START_PAGE_DEF   = "1";  // 1st page

// Optional parameter
const OS_PARAM_OPTIONAL     = /\{(?:\w+:)?\w+\?\}/g;

// A array of arrays containing parameters that we don't fully support, and
// their default values. We will only send values for these parameters if
// required, since our values are just really arbitrary "guesses" that should
// give us the output we want.
var OS_UNSUPPORTED_PARAMS = [
  [OS_PARAM_COUNT, OS_PARAM_COUNT_DEF],
  [OS_PARAM_START_INDEX, OS_PARAM_START_INDEX_DEF],
  [OS_PARAM_START_PAGE, OS_PARAM_START_PAGE_DEF],
];

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const SEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// Returns false for whitespace-only or commented out lines in a
// Sherlock file, true otherwise.
function isUsefulLine(aLine) {
  return !(/^\s*($|#)/i.test(aLine));
}

__defineGetter__("FileUtils", function() {
  delete this.FileUtils;
  Components.utils.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

__defineGetter__("NetUtil", function() {
  delete this.NetUtil;
  Components.utils.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

__defineGetter__("gChromeReg", function() {
  delete this.gChromeReg;
  return this.gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                           getService(Ci.nsIChromeRegistry);
});

/**
 * Prefixed to all search debug output.
 */
const SEARCH_LOG_PREFIX = "*** Search: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout.
 */
function DO_LOG(aText) {
  dump(SEARCH_LOG_PREFIX + aText + "\n");
  Services.console.logStringMessage(aText);
}

#ifdef DEBUG
/**
 * In debug builds, use a live, pref-based (browser.search.log) LOG function
 * to allow enabling/disabling without a restart.
 */
function PREF_LOG(aText) {
  if (getBoolPref(BROWSER_SEARCH_PREF + "log", false))
    DO_LOG(aText);
}
var LOG = PREF_LOG;

#else

/**
 * Otherwise, don't log at all by default. This can be overridden at startup
 * by the pref, see SearchService's _init method.
 */
var LOG = function(){};

#endif

/**
 * Presents an assertion dialog in non-release builds and throws.
 * @param  message
 *         A message to display
 * @param  resultCode
 *         The NS_ERROR_* value to throw.
 * @throws resultCode
 */
function ERROR(message, resultCode) {
  NS_ASSERT(false, SEARCH_LOG_PREFIX + message);
  throw Components.Exception(message, resultCode);
}

/**
 * Logs the failure message (if browser.search.log is enabled) and throws.
 * @param  message
 *         A message to display
 * @param  resultCode
 *         The NS_ERROR_* value to throw.
 * @throws resultCode or NS_ERROR_INVALID_ARG if resultCode isn't specified.
 */
function FAIL(message, resultCode) {
  LOG(message);
  throw Components.Exception(message, resultCode || Cr.NS_ERROR_INVALID_ARG);
}

/**
 * Ensures an assertion is met before continuing. Should be used to indicate
 * fatal errors.
 * @param  assertion
 *         An assertion that must be met
 * @param  message
 *         A message to display if the assertion is not met
 * @param  resultCode
 *         The NS_ERROR_* value to throw if the assertion is not met
 * @throws resultCode
 */
function ENSURE_WARN(assertion, message, resultCode) {
  NS_ASSERT(assertion, SEARCH_LOG_PREFIX + message);
  if (!assertion)
    throw Components.Exception(message, resultCode);
}

/**
 * A delayed treatment that may be delayed even further.
 *
 * Use this for instance if you write data to a file and you expect
 * that you may have to rewrite data very soon afterwards. With
 * |Lazy|, the treatment is delayed by a few milliseconds and,
 * should a new change to the data occur during this period,
 * 1/ only the final version of the data is actually written;
 * 2/ a further grace delay is added to take into account other
 * changes.
 *
 * @constructor
 * @param {Function} code The code to execute after the delay.
 * @param {number=} delay An optional delay, in milliseconds.
 */
function Lazy(code, delay) {
  LOG("Lazy: Creating a Lazy");
  this._callback =
    (function(){
       code();
       this._timer = null;
     }).bind(this);
  this._delay = delay || LAZY_SERIALIZE_DELAY;
  this._timer = null;
}
Lazy.prototype = {
  /**
   * Start (or postpone) treatment.
   */
  go: function Lazy_go() {
    LOG("Lazy_go: starting");
    if (this._timer) {
      LOG("Lazy_go: reusing active timer");
      this._timer.delay = this._delay;
    } else {
      LOG("Lazy_go: creating timer");
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.
        initWithCallback(this._callback,
                         this._delay,
                         Ci.nsITimer.TYPE_ONE_SHOT);
    }
  },
  /**
   * Perform any postponed treatment immediately.
   */
  flush: function Lazy_flush() {
    LOG("Lazy_flush: starting");
    if (!this._timer) {
      return;
    }
    this._timer.cancel();
    this._timer = null;
    this._callback();
  }
};


function loadListener(aChannel, aEngine, aCallback) {
  this._channel = aChannel;
  this._bytes = [];
  this._engine = aEngine;
  this._callback = aCallback;
}
loadListener.prototype = {
  _callback: null,
  _channel: null,
  _countRead: 0,
  _engine: null,
  _stream: null,

  QueryInterface: function SRCH_loadQI(aIID) {
    if (aIID.equals(Ci.nsISupports)           ||
        aIID.equals(Ci.nsIRequestObserver)    ||
        aIID.equals(Ci.nsIStreamListener)     ||
        aIID.equals(Ci.nsIChannelEventSink)   ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        aIID.equals(Ci.nsIBadCertListener2)   ||
        aIID.equals(Ci.nsISSLErrorListener)   ||
        // See FIXME comment below
        aIID.equals(Ci.nsIHttpEventSink)      ||
        aIID.equals(Ci.nsIProgressEventSink)  ||
        false)
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  // nsIRequestObserver
  onStartRequest: function SRCH_loadStartR(aRequest, aContext) {
    LOG("loadListener: Starting request: " + aRequest.name);
    this._stream = Cc["@mozilla.org/binaryinputstream;1"].
                   createInstance(Ci.nsIBinaryInputStream);
  },

  onStopRequest: function SRCH_loadStopR(aRequest, aContext, aStatusCode) {
    LOG("loadListener: Stopping request: " + aRequest.name);

    var requestFailed = !Components.isSuccessCode(aStatusCode);
    if (!requestFailed && (aRequest instanceof Ci.nsIHttpChannel))
      requestFailed = !aRequest.requestSucceeded;

    if (requestFailed || this._countRead == 0) {
      LOG("loadListener: request failed!");
      // send null so the callback can deal with the failure
      this._callback(null, this._engine);
    } else
      this._callback(this._bytes, this._engine);
    this._channel = null;
    this._engine  = null;
  },

  // nsIStreamListener
  onDataAvailable: function SRCH_loadDAvailable(aRequest, aContext,
                                                aInputStream, aOffset,
                                                aCount) {
    this._stream.setInputStream(aInputStream);

    // Get a byte array of the data
    this._bytes = this._bytes.concat(this._stream.readByteArray(aCount));
    this._countRead += aCount;
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect: function SRCH_loadCRedirect(aOldChannel, aNewChannel,
                                                      aFlags, callback) {
    this._channel = aNewChannel;
    callback.onRedirectVerifyCallback(Components.results.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface: function SRCH_load_GI(aIID) {
    return this.QueryInterface(aIID);
  },

  // nsIBadCertListener2
  notifyCertProblem: function SRCH_certProblem(socketInfo, status, targetSite) {
    return true;
  },

  // nsISSLErrorListener
  notifySSLError: function SRCH_SSLError(socketInfo, error, targetSite) {
    return true;
  },

  // FIXME: bug 253127
  // nsIHttpEventSink
  onRedirect: function (aChannel, aNewChannel) {},
  // nsIProgressEventSink
  onProgress: function (aRequest, aContext, aProgress, aProgressMax) {},
  onStatus: function (aRequest, aContext, aStatus, aStatusArg) {}
}


/**
 * Used to verify a given DOM node's localName and namespaceURI.
 * @param aElement
 *        The element to verify.
 * @param aLocalNameArray
 *        An array of strings to compare against aElement's localName.
 * @param aNameSpaceArray
 *        An array of strings to compare against aElement's namespaceURI.
 *
 * @returns false if aElement is null, or if its localName or namespaceURI
 *          does not match one of the elements in the aLocalNameArray or
 *          aNameSpaceArray arrays, respectively.
 * @throws NS_ERROR_INVALID_ARG if aLocalNameArray or aNameSpaceArray are null.
 */
function checkNameSpace(aElement, aLocalNameArray, aNameSpaceArray) {
  if (!aLocalNameArray || !aNameSpaceArray)
    FAIL("missing aLocalNameArray or aNameSpaceArray for checkNameSpace");
  return (aElement                                                &&
          (aLocalNameArray.indexOf(aElement.localName)    != -1)  &&
          (aNameSpaceArray.indexOf(aElement.namespaceURI) != -1));
}

/**
 * Safely close a nsISafeOutputStream.
 * @param aFOS
 *        The file output stream to close.
 */
function closeSafeOutputStream(aFOS) {
  if (aFOS instanceof Ci.nsISafeOutputStream) {
    try {
      aFOS.finish();
      return;
    } catch (e) { }
  }
  aFOS.close();
}

/**
 * Wrapper function for nsIIOService::newURI.
 * @param aURLSpec
 *        The URL string from which to create an nsIURI.
 * @returns an nsIURI object, or null if the creation of the URI failed.
 */
function makeURI(aURLSpec, aCharset) {
  try {
    return NetUtil.newURI(aURLSpec, aCharset);
  } catch (ex) { }

  return null;
}

/**
 * Gets a directory from the directory service.
 * @param aKey
 *        The directory service key indicating the directory to get.
 */
function getDir(aKey, aIFace) {
  if (!aKey)
    FAIL("getDir requires a directory key!");

  return Services.dirsvc.get(aKey, aIFace || Ci.nsIFile);
}

/**
 * The following two functions are essentially copied from
 * nsInternetSearchService. They are required for backwards compatibility.
 */
function queryCharsetFromCode(aCode) {
  const codes = [];
  codes[0] = "x-mac-roman";
  codes[6] = "x-mac-greek";
  codes[35] = "x-mac-turkish";
  codes[513] = "ISO-8859-1";
  codes[514] = "ISO-8859-2";
  codes[517] = "ISO-8859-5";
  codes[518] = "ISO-8859-6";
  codes[519] = "ISO-8859-7";
  codes[520] = "ISO-8859-8";
  codes[521] = "ISO-8859-9";
  codes[1049] = "IBM864";
  codes[1280] = "windows-1252";
  codes[1281] = "windows-1250";
  codes[1282] = "windows-1251";
  codes[1283] = "windows-1253";
  codes[1284] = "windows-1254";
  codes[1285] = "windows-1255";
  codes[1286] = "windows-1256";
  codes[1536] = "us-ascii";
  codes[1584] = "GB2312";
  codes[1585] = "gbk";
  codes[1600] = "EUC-KR";
  codes[2080] = "ISO-2022-JP";
  codes[2096] = "ISO-2022-CN";
  codes[2112] = "ISO-2022-KR";
  codes[2336] = "EUC-JP";
  codes[2352] = "GB2312";
  codes[2353] = "x-euc-tw";
  codes[2368] = "EUC-KR";
  codes[2561] = "Shift_JIS";
  codes[2562] = "KOI8-R";
  codes[2563] = "Big5";
  codes[2565] = "HZ-GB-2312";

  if (codes[aCode])
    return codes[aCode];

  return getLocalizedPref("intl.charset.default", DEFAULT_QUERY_CHARSET);
}
function fileCharsetFromCode(aCode) {
  const codes = [
    "x-mac-roman",           // 0
    "Shift_JIS",             // 1
    "Big5",                  // 2
    "EUC-KR",                // 3
    "X-MAC-ARABIC",          // 4
    "X-MAC-HEBREW",          // 5
    "X-MAC-GREEK",           // 6
    "X-MAC-CYRILLIC",        // 7
    "X-MAC-DEVANAGARI" ,     // 9
    "X-MAC-GURMUKHI",        // 10
    "X-MAC-GUJARATI",        // 11
    "X-MAC-ORIYA",           // 12
    "X-MAC-BENGALI",         // 13
    "X-MAC-TAMIL",           // 14
    "X-MAC-TELUGU",          // 15
    "X-MAC-KANNADA",         // 16
    "X-MAC-MALAYALAM",       // 17
    "X-MAC-SINHALESE",       // 18
    "X-MAC-BURMESE",         // 19
    "X-MAC-KHMER",           // 20
    "X-MAC-THAI",            // 21
    "X-MAC-LAOTIAN",         // 22
    "X-MAC-GEORGIAN",        // 23
    "X-MAC-ARMENIAN",        // 24
    "GB2312",                // 25
    "X-MAC-TIBETAN",         // 26
    "X-MAC-MONGOLIAN",       // 27
    "X-MAC-ETHIOPIC",        // 28
    "X-MAC-CENTRALEURROMAN", // 29
    "X-MAC-VIETNAMESE",      // 30
    "X-MAC-EXTARABIC"        // 31
  ];
  // Sherlock files have always defaulted to x-mac-roman, so do that here too
  return codes[aCode] || codes[0];
}

/**
 * Returns a string interpretation of aBytes using aCharset, or null on
 * failure.
 */
function bytesToString(aBytes, aCharset) {
  var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  LOG("bytesToString: converting using charset: " + aCharset);

  try {
    converter.charset = aCharset;
    return converter.convertFromByteArray(aBytes, aBytes.length);
  } catch (ex) {}

  return null;
}

/**
 * Converts an array of bytes representing a Sherlock file into an array of
 * lines representing the useful data from the file.
 *
 * @param aBytes
 *        The array of bytes representing the Sherlock file.
 * @param aCharsetCode
 *        An integer value representing a character set code to be passed to
 *        fileCharsetFromCode, or null for the default Sherlock encoding.
 */
function sherlockBytesToLines(aBytes, aCharsetCode) {
  // fileCharsetFromCode returns the default encoding if aCharsetCode is null
  var charset = fileCharsetFromCode(aCharsetCode);

  var dataString = bytesToString(aBytes, charset);
  if (!dataString)
    FAIL("sherlockBytesToLines: Couldn't convert byte array!", Cr.NS_ERROR_FAILURE);

  // Split the string into lines, and filter out comments and
  // whitespace-only lines
  return dataString.split(NEW_LINES).filter(isUsefulLine);
}

/**
 * Gets the current value of the locale.  It's possible for this preference to
 * be localized, so we have to do a little extra work here.  Similar code
 * exists in nsHttpHandler.cpp when building the UA string.
 */
function getLocale() {
  const localePref = "general.useragent.locale";
  var locale = getLocalizedPref(localePref);
  if (locale)
    return locale;

  // Not localized
  return Services.prefs.getCharPref(localePref);
}

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getLocalizedPref(aPrefName, aDefault) {
  const nsIPLS = Ci.nsIPrefLocalizedString;
  try {
    return Services.prefs.getComplexValue(aPrefName, nsIPLS).data;
  } catch (ex) {}

  return aDefault;
}

/**
 * Wrapper for nsIPrefBranch::setComplexValue.
 * @param aPrefName
 *        The name of the pref to set.
 */
function setLocalizedPref(aPrefName, aValue) {
  const nsIPLS = Ci.nsIPrefLocalizedString;
  try {
    var pls = Components.classes["@mozilla.org/pref-localizedstring;1"]
                        .createInstance(Ci.nsIPrefLocalizedString);
    pls.data = aValue;
    Services.prefs.setComplexValue(aPrefName, nsIPLS, pls);
  } catch (ex) {}
}

/**
 * Wrapper for nsIPrefBranch::getBoolPref.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getBoolPref(aName, aDefault) {
  try {
    return Services.prefs.getBoolPref(aName);
  } catch (ex) {
    return aDefault;
  }
}

/**
 * Get a unique nsIFile object with a sanitized name, based on the engine name.
 * @param aName
 *        A name to "sanitize". Can be an empty string, in which case a random
 *        8 character filename will be produced.
 * @returns A nsIFile object in the user's search engines directory with a
 *          unique sanitized name.
 */
function getSanitizedFile(aName) {
  var fileName = sanitizeName(aName) + "." + XML_FILE_EXT;
  var file = getDir(NS_APP_USER_SEARCH_DIR);
  file.append(fileName);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  return file;
}

/**
 * @return a sanitized name to be used as a filename, or a random name
 *         if a sanitized name cannot be obtained (if aName contains
 *         no valid characters).
 */
function sanitizeName(aName) {
  const maxLength = 60;
  const minLength = 1;
  var name = aName.toLowerCase();
  name = name.replace(/\s+/g, "-");
  name = name.replace(/[^-a-z0-9]/g, "");

  // Use a random name if our input had no valid characters.
  if (name.length < minLength)
    name = Math.random().toString(36).replace(/^.*\./, '');

  // Force max length.
  return name.substring(0, maxLength);
}

/**
 * Retrieve a pref from the search param branch.
 *
 * @param prefName
 *        The name of the pref.
 **/
function getMozParamPref(prefName)
  Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "param." + prefName);

/**
 * Notifies watchers of SEARCH_ENGINE_TOPIC about changes to an engine or to
 * the state of the search service.
 *
 * @param aEngine
 *        The nsISearchEngine object to which the change applies.
 * @param aVerb
 *        A verb describing the change.
 *
 * @see nsIBrowserSearchService.idl
 */
let gEnginesLoaded = false;
function notifyAction(aEngine, aVerb) {
  if (gEnginesLoaded) {
    LOG("NOTIFY: Engine: \"" + aEngine.name + "\"; Verb: \"" + aVerb + "\"");
    Services.obs.notifyObservers(aEngine, SEARCH_ENGINE_TOPIC, aVerb);
  }
}

function  parseJsonFromStream(aInputStream) {
  const json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
  const data = json.decodeFromStream(aInputStream, aInputStream.available());
  return data;
}

/**
 * Simple object representing a name/value pair.
 */
function QueryParameter(aName, aValue) {
  if (!aName || (aValue == null))
    FAIL("missing name or value for QueryParameter!");

  this.name = aName;
  this.value = aValue;
}

/**
 * Perform OpenSearch parameter substitution on aParamValue.
 *
 * @param aParamValue
 *        A string containing OpenSearch search parameters.
 * @param aSearchTerms
 *        The user-provided search terms. This string will inserted into
 *        aParamValue as the value of the OS_PARAM_USER_DEFINED parameter.
 *        This value must already be escaped appropriately - it is inserted
 *        as-is.
 * @param aEngine
 *        The engine which owns the string being acted on.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#core
 */
function ParamSubstitution(aParamValue, aSearchTerms, aEngine) {
  var value = aParamValue;

  var distributionID = MOZ_DISTRIBUTION_ID;
  try {
    distributionID = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "distributionID");
  }
  catch (ex) { }

  // Custom search parameters. These are only available to default search
  // engines.
  if (aEngine._isDefault) {
    value = value.replace(MOZ_PARAM_LOCALE, getLocale());
    value = value.replace(MOZ_PARAM_DIST_ID, distributionID);
    value = value.replace(MOZ_PARAM_OFFICIAL, MOZ_OFFICIAL);
  }

  // Insert the OpenSearch parameters we're confident about
  value = value.replace(OS_PARAM_USER_DEFINED, aSearchTerms);
  value = value.replace(OS_PARAM_INPUT_ENCODING, aEngine.queryCharset);
  value = value.replace(OS_PARAM_LANGUAGE,
                        getLocale() || OS_PARAM_LANGUAGE_DEF);
  value = value.replace(OS_PARAM_OUTPUT_ENCODING,
                        OS_PARAM_OUTPUT_ENCODING_DEF);

  // Replace any optional parameters
  value = value.replace(OS_PARAM_OPTIONAL, "");

  // Insert any remaining required params with our default values
  for (var i = 0; i < OS_UNSUPPORTED_PARAMS.length; ++i) {
    value = value.replace(OS_UNSUPPORTED_PARAMS[i][0],
                          OS_UNSUPPORTED_PARAMS[i][1]);
  }

  return value;
}

/**
 * Creates an engineURL object, which holds the query URL and all parameters.
 *
 * @param aType
 *        A string containing the name of the MIME type of the search results
 *        returned by this URL.
 * @param aMethod
 *        The HTTP request method. Must be a case insensitive value of either
 *        "GET" or "POST".
 * @param aTemplate
 *        The URL to which search queries should be sent. For GET requests,
 *        must contain the string "{searchTerms}", to indicate where the user
 *        entered search terms should be inserted.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
 *
 * @throws NS_ERROR_NOT_IMPLEMENTED if aType is unsupported.
 */
function EngineURL(aType, aMethod, aTemplate) {
  if (!aType || !aMethod || !aTemplate)
    FAIL("missing type, method or template for EngineURL!");

  var method = aMethod.toUpperCase();
  var type   = aType.toLowerCase();

  if (method != "GET" && method != "POST")
    FAIL("method passed to EngineURL must be \"GET\" or \"POST\"");

  this.type     = type;
  this.method   = method;
  this.params   = [];
  this.rels     = [];
  // Don't serialize expanded mozparams
  this.mozparams = {};

  var templateURI = makeURI(aTemplate);
  if (!templateURI)
    FAIL("new EngineURL: template is not a valid URI!", Cr.NS_ERROR_FAILURE);

  switch (templateURI.scheme) {
    case "http":
    case "https":
    // Disable these for now, see bug 295018
    // case "file":
    // case "resource":
      this.template = aTemplate;
      break;
    default:
      FAIL("new EngineURL: template uses invalid scheme!", Cr.NS_ERROR_FAILURE);
  }
}
EngineURL.prototype = {

  addParam: function SRCH_EURL_addParam(aName, aValue) {
    this.params.push(new QueryParameter(aName, aValue));
  },

  _addMozParam: function SRCH_EURL__addMozParam(aObj) {
    aObj.mozparam = true;
    this.mozparams[aObj.name] = aObj;
  },

  getSubmission: function SRCH_EURL_getSubmission(aSearchTerms, aEngine) {
    var url = ParamSubstitution(this.template, aSearchTerms, aEngine);

    // Create an application/x-www-form-urlencoded representation of our params
    // (name=value&name=value&name=value)
    var dataString = "";
    for (var i = 0; i < this.params.length; ++i) {
      var param = this.params[i];
      var value = ParamSubstitution(param.value, aSearchTerms, aEngine);

      dataString += (i > 0 ? "&" : "") + param.name + "=" + value;
    }

    var postData = null;
    if (this.method == "GET") {
      // GET method requests have no post data, and append the encoded
      // query string to the url...
      if (url.indexOf("?") == -1 && dataString)
        url += "?";
      url += dataString;
    } else if (this.method == "POST") {
      // POST method requests must wrap the encoded text in a MIME
      // stream and supply that as POSTDATA.
      var stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                         createInstance(Ci.nsIStringInputStream);
      stringStream.data = dataString;

      postData = Cc["@mozilla.org/network/mime-input-stream;1"].
                 createInstance(Ci.nsIMIMEInputStream);
      postData.addHeader("Content-Type", "application/x-www-form-urlencoded");
      postData.addContentLength = true;
      postData.setData(stringStream);
    }

    return new Submission(makeURI(url), postData);
  },

  _hasRelation: function SRC_EURL__hasRelation(aRel)
    this.rels.some(function(e) e == aRel.toLowerCase()),

  _initWithJSON: function SRC_EURL__initWithJSON(aJson, aEngine) {
    if (!aJson.params)
      return;

    this.rels = aJson.rels;

    for (let i = 0; i < aJson.params.length; ++i) {
      let param = aJson.params[i];
      if (param.mozparam) {
        if (param.condition == "defaultEngine") {
          if (aEngine._isDefaultEngine())
            this.addParam(param.name, param.trueValue);
          else
            this.addParam(param.name, param.falseValue);
        } else if (param.condition == "pref") {
          let value = getMozParamPref(param.pref);
          this.addParam(param.name, value);
        }
        this._addMozParam(param);
      }
      else
        this.addParam(param.name, param.value);
    }
  },

  /**
   * Creates a JavaScript object that represents this URL.
   * @returns An object suitable for serialization as JSON.
   **/
  _serializeToJSON: function SRCH_EURL__serializeToJSON() {
    var json = {
      template: this.template,
      rels: this.rels
    };

    if (this.type != URLTYPE_SEARCH_HTML)
      json.type = this.type;
    if (this.method != "GET")
      json.method = this.method;

    function collapseMozParams(aParam)
      this.mozparams[aParam.name] || aParam;
    json.params = this.params.map(collapseMozParams, this);

    return json;
  },

  /**
   * Serializes the engine object to a OpenSearch Url element.
   * @param aDoc
   *        The document to use to create the Url element.
   * @param aElement
   *        The element to which the created Url element is appended.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
   */
  _serializeToElement: function SRCH_EURL_serializeToEl(aDoc, aElement) {
    var url = aDoc.createElementNS(OPENSEARCH_NS_11, "Url");
    url.setAttribute("type", this.type);
    url.setAttribute("method", this.method);
    url.setAttribute("template", this.template);
    if (this.rels.length)
      url.setAttribute("rel", this.rels.join(" "));

    for (var i = 0; i < this.params.length; ++i) {
      var param = aDoc.createElementNS(OPENSEARCH_NS_11, "Param");
      param.setAttribute("name", this.params[i].name);
      param.setAttribute("value", this.params[i].value);
      url.appendChild(aDoc.createTextNode("\n  "));
      url.appendChild(param);
    }
    url.appendChild(aDoc.createTextNode("\n"));
    aElement.appendChild(url);
  }
};

/**
 * nsISearchEngine constructor.
 * @param aLocation
 *        A nsILocalFile or nsIURI object representing the location of the
 *        search engine data file.
 * @param aSourceDataType
 *        The data type of the file used to describe the engine. Must be either
 *        DATA_XML or DATA_TEXT.
 * @param aIsReadOnly
 *        Boolean indicating whether the engine should be treated as read-only.
 *        Read only engines cannot be serialized to file.
 */
function Engine(aLocation, aSourceDataType, aIsReadOnly) {
  this._dataType = aSourceDataType;
  this._readOnly = aIsReadOnly;
  this._urls = [];

  if (aLocation.type) {
    if (aLocation.type == "filePath")
      this._file = aLocation.value;
    else if (aLocation.type == "uri")
      this._uri = aLocation.value;
  } else if (aLocation instanceof Ci.nsILocalFile) {
    // we already have a file (e.g. loading engines from disk)
    this._file = aLocation;
  } else if (aLocation instanceof Ci.nsIURI) {
    switch (aLocation.scheme) {
      case "https":
      case "http":
      case "ftp":
      case "data":
      case "file":
      case "resource":
      case "chrome":
        this._uri = aLocation;
        break;
      default:
        ERROR("Invalid URI passed to the nsISearchEngine constructor",
              Cr.NS_ERROR_INVALID_ARG);
    }
  } else
    ERROR("Engine location is neither a File nor a URI object",
          Cr.NS_ERROR_INVALID_ARG);
}

Engine.prototype = {
  // The engine's alias (can be null). Initialized to |undefined| to indicate
  // not-initialized-from-engineMetadataService.
  _alias: undefined,
  // The data describing the engine. Is either an array of bytes, for Sherlock
  // files, or an XML document element, for XML plugins.
  _data: null,
  // The engine's data type. See data types (DATA_) defined above.
  _dataType: null,
  // Whether or not the engine is readonly.
  _readOnly: true,
  // The engine's description
  _description: "",
  // Used to store the engine to replace, if we're an update to an existing
  // engine.
  _engineToUpdate: null,
  // The file from which the plugin was loaded.
  __file: null,
  get _file() {
    if (this.__file && !(this.__file instanceof Ci.nsILocalFile)) {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.persistentDescriptor = this.__file;
      return this.__file = file;
    }
    return this.__file;
  },
  set _file(aValue) {
    this.__file = aValue;
  },
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon: null,
  // Whether the engine is hidden from the user.
  _hidden: null,
  // The engine's name.
  _name: null,
  // The engine type. See engine types (TYPE_) defined above.
  _type: null,
  // The name of the charset used to submit the search terms.
  _queryCharset: null,
  // A URL string pointing to the engine's search form.
  __searchForm: null,
  get _searchForm() {
    return this.__searchForm;
  },
  set _searchForm(aValue) {
    if (/^https?:/i.test(aValue))
      this.__searchForm = aValue;
    else
      LOG("_searchForm: Invalid URL dropped for " + this._name ||
          "the current engine");
  },
  // The URI object from which the engine was retrieved.
  // This is null for engines loaded from disk, but present for engines loaded
  // from chrome:// URIs.
  __uri: null,
  get _uri() {
    if (this.__uri && !(this.__uri instanceof Ci.nsIURI))
      this.__uri = makeURI(this.__uri);

    return this.__uri;
  },
  set _uri(aValue) {
    this.__uri = aValue;
  },
  // Whether to obtain user confirmation before adding the engine. This is only
  // used when the engine is first added to the list.
  _confirm: false,
  // Whether to set this as the current engine as soon as it is loaded.  This
  // is only used when the engine is first added to the list.
  _useNow: true,
  // Where the engine was loaded from. Can be one of: SEARCH_APP_DIR,
  // SEARCH_PROFILE_DIR, SEARCH_IN_EXTENSION.
  __installLocation: null,
  // The number of days between update checks for new versions
  _updateInterval: null,
  // The url to check at for a new update
  _updateURL: null,
  // The url to check for a new icon
  _iconUpdateURL: null,
  // A reference to the timer used for lazily serializing the engine to file
  _serializeTimer: null,
  // Whether this engine has been used since the cache was last recreated.
  __used: null,
  get _used() {
    if (!this.__used)
      this.__used = !!engineMetadataService.getAttr(this, "used");
    return this.__used;
  },
  set _used(aValue) {
    this.__used = aValue
    engineMetadataService.setAttr(this, "used", aValue);
  },

  /**
   * Retrieves the data from the engine's file. If the engine's dataType is
   * XML, the document element is placed in the engine's data field. For text
   * engines, the data is just read directly from file and placed as an array
   * of lines in the engine's data field.
   */
  _initFromFile: function SRCH_ENG_initFromFile() {
    if (!this._file || !this._file.exists())
      FAIL("File must exist before calling initFromFile!", Cr.NS_ERROR_UNEXPECTED);

    var fileInStream = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);

    fileInStream.init(this._file, MODE_RDONLY, PERMS_FILE, false);

    switch (this._dataType) {
      case SEARCH_DATA_XML:
        var domParser = Cc["@mozilla.org/xmlextras/domparser;1"].
                        createInstance(Ci.nsIDOMParser);
        var doc = domParser.parseFromStream(fileInStream, "UTF-8",
                                            this._file.fileSize,
                                            "text/xml");

        this._data = doc.documentElement;
        break;
      case SEARCH_DATA_TEXT:
        var binaryInStream = Cc["@mozilla.org/binaryinputstream;1"].
                             createInstance(Ci.nsIBinaryInputStream);
        binaryInStream.setInputStream(fileInStream);

        var bytes = binaryInStream.readByteArray(binaryInStream.available());
        this._data = bytes;

        break;
      default:
        ERROR("Bogus engine _dataType: \"" + this._dataType + "\"",
              Cr.NS_ERROR_UNEXPECTED);
    }
    fileInStream.close();

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the engine data from a URI.
   */
  _initFromURI: function SRCH_ENG_initFromURI() {
    ENSURE_WARN(this._uri instanceof Ci.nsIURI,
                "Must have URI when calling _initFromURI!",
                Cr.NS_ERROR_UNEXPECTED);

    LOG("_initFromURI: Downloading engine from: \"" + this._uri.spec + "\".");

    var chan = NetUtil.ioService.newChannelFromURI(this._uri);

    if (this._engineToUpdate && (chan instanceof Ci.nsIHttpChannel)) {
      var lastModified = engineMetadataService.getAttr(this._engineToUpdate,
                                                       "updatelastmodified");
      if (lastModified)
        chan.setRequestHeader("If-Modified-Since", lastModified, false);
    }
    var listener = new loadListener(chan, this, this._onLoad);
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener, null);
  },
  
  _initFromURISync: function SRCH_ENG_initFromURISync() {
    ENSURE_WARN(this._uri instanceof Ci.nsIURI,
                "Must have URI when calling _initFromURISync!",
                Cr.NS_ERROR_UNEXPECTED);

    ENSURE_WARN(this._uri.schemeIs("chrome"), "_initFromURISync called for non-chrome URI",
                Cr.NS_ERROR_FAILURE);

    LOG("_initFromURISync: Loading engine from: \"" + this._uri.spec + "\".");

    var chan = NetUtil.ioService.newChannelFromURI(this._uri);

    var stream = chan.open();
    var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    var doc = parser.parseFromStream(stream, "UTF-8", stream.available(), "text/xml");

    this._data = doc.documentElement;

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Attempts to find an EngineURL object in the set of EngineURLs for
   * this Engine that has the given type string.  (This corresponds to the
   * "type" attribute in the "Url" node in the OpenSearch spec.)
   * This method will return the first matching URL object found, or null
   * if no matching URL is found.
   *
   * @param aType string to match the EngineURL's type attribute
   */
  _getURLOfType: function SRCH_ENG__getURLOfType(aType) {
    for (var i = 0; i < this._urls.length; ++i) {
      if (this._urls[i].type == aType)
        return this._urls[i];
    }

    return null;
  },

  _confirmAddEngine: function SRCH_SVC_confirmAddEngine() {
    var stringBundle = Services.strings.createBundle(SEARCH_BUNDLE);
    var titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");

    // Display only the hostname portion of the URL.
    var dialogMessage =
        stringBundle.formatStringFromName("addEngineConfirmation",
                                          [this._name, this._uri.host], 2);
    var checkboxMessage = null;
    if (!getBoolPref(BROWSER_SEARCH_PREF + "noCurrentEngine", false))
      checkboxMessage = stringBundle.GetStringFromName("addEngineUseNowText");

    var addButtonLabel =
        stringBundle.GetStringFromName("addEngineAddButtonLabel");

    var ps = Services.prompt;
    var buttonFlags = (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0) +
                      (ps.BUTTON_TITLE_CANCEL    * ps.BUTTON_POS_1) +
                       ps.BUTTON_POS_0_DEFAULT;

    var checked = {value: false};
    // confirmEx returns the index of the button that was pressed.  Since "Add"
    // is button 0, we want to return the negation of that value.
    var confirm = !ps.confirmEx(null,
                                titleMessage,
                                dialogMessage,
                                buttonFlags,
                                addButtonLabel,
                                null, null, // button 1 & 2 names not used
                                checkboxMessage,
                                checked);

    return {confirmed: confirm, useNow: checked.value};
  },

  /**
   * Handle the successful download of an engine. Initializes the engine and
   * triggers parsing of the data. The engine is then flushed to disk. Notifies
   * the search service once initialization is complete.
   */
  _onLoad: function SRCH_ENG_onLoad(aBytes, aEngine) {
    /**
     * Handle an error during the load of an engine by prompting the user to
     * notify him that the load failed.
     */
    function onError(aErrorString, aTitleString) {
      if (aEngine._engineToUpdate) {
        // We're in an update, so just fail quietly
        LOG("updating " + aEngine._engineToUpdate.name + " failed");
        return;
      }
      var brandBundle = Services.strings.createBundle(BRAND_BUNDLE);
      var brandName = brandBundle.GetStringFromName("brandShortName");

      var searchBundle = Services.strings.createBundle(SEARCH_BUNDLE);
      var msgStringName = aErrorString || "error_loading_engine_msg2";
      var titleStringName = aTitleString || "error_loading_engine_title";
      var title = searchBundle.GetStringFromName(titleStringName);
      var text = searchBundle.formatStringFromName(msgStringName,
                                                   [brandName, aEngine._location],
                                                   2);

      Services.ww.getNewPrompter(null).alert(title, text);
    }

    if (!aBytes) {
      onError();
      return;
    }

    var engineToUpdate = null;
    if (aEngine._engineToUpdate) {
      engineToUpdate = aEngine._engineToUpdate.wrappedJSObject;

      // Make this new engine use the old engine's file.
      aEngine._file = engineToUpdate._file;
    }

    switch (aEngine._dataType) {
      case SEARCH_DATA_XML:
        var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                     createInstance(Ci.nsIDOMParser);
        var doc = parser.parseFromBuffer(aBytes, aBytes.length, "text/xml");
        aEngine._data = doc.documentElement;
        break;
      case SEARCH_DATA_TEXT:
        aEngine._data = aBytes;
        break;
      default:
        onError();
        LOG("_onLoad: Bogus engine _dataType: \"" + this._dataType + "\"");
        return;
    }

    try {
      // Initialize the engine from the obtained data
      aEngine._initFromData();
    } catch (ex) {
      LOG("_onLoad: Failed to init engine!\n" + ex);
      // Report an error to the user
      onError();
      return;
    }

    // Check to see if this is a duplicate engine. If we're confirming the
    // engine load, then we display a "this is a duplicate engine" prompt,
    // otherwise we fail silently.
    if (!engineToUpdate) {
      if (Services.search.getEngineByName(aEngine.name)) {
        if (aEngine._confirm)
          onError("error_duplicate_engine_msg", "error_invalid_engine_title");

        LOG("_onLoad: duplicate engine found, bailing");
        return;
      }
    }

    // If requested, confirm the addition now that we have the title.
    // This property is only ever true for engines added via
    // nsIBrowserSearchService::addEngine.
    if (aEngine._confirm) {
      var confirmation = aEngine._confirmAddEngine();
      LOG("_onLoad: confirm is " + confirmation.confirmed +
          "; useNow is " + confirmation.useNow);
      if (!confirmation.confirmed)
        return;
      aEngine._useNow = confirmation.useNow;
    }

    // If we don't yet have a file, get one now. The only case where we would
    // already have a file is if this is an update and _file was set above.
    if (!aEngine._file)
      aEngine._file = getSanitizedFile(aEngine.name);

    if (engineToUpdate) {
      // Keep track of the last modified date, so that we can make conditional
      // requests for future updates.
      engineMetadataService.setAttr(aEngine, "updatelastmodified",
                                    (new Date()).toUTCString());

      // If we're updating an app-shipped engine, ensure that the updateURLs
      // are the same.
      if (engineToUpdate._isInAppDir) {
        let oldUpdateURL = engineToUpdate._updateURL;
        let newUpdateURL = aEngine._updateURL;
        let oldSelfURL = engineToUpdate._getURLOfType(URLTYPE_OPENSEARCH);
        if (oldSelfURL && oldSelfURL._hasRelation("self")) {
          oldUpdateURL = oldSelfURL.template;
          let newSelfURL = aEngine._getURLOfType(URLTYPE_OPENSEARCH);
          if (!newSelfURL || !newSelfURL._hasRelation("self")) {
            LOG("_onLoad: updateURL missing in updated engine for " +
                aEngine.name + " aborted");
            return;
          }
          newUpdateURL = newSelfURL.template;
        }

        if (oldUpdateURL != newUpdateURL) {
          LOG("_onLoad: updateURLs do not match! Update of " + aEngine.name + " aborted");
          return;
        }
      }

      // Set the new engine's icon, if it doesn't yet have one.
      if (!aEngine._iconURI && engineToUpdate._iconURI)
        aEngine._iconURI = engineToUpdate._iconURI;

      // Clear the "use now" flag since we don't want to be changing the
      // current engine for an update.
      aEngine._useNow = false;
    }

    // Write the engine to file. For readOnly engines, they'll be stored in the
    // cache following the notification below.
    if (!aEngine._readOnly)
      aEngine._serializeToFile();

    // Notify the search service of the successful load. It will deal with
    // updates by checking aEngine._engineToUpdate.
    notifyAction(aEngine, SEARCH_ENGINE_LOADED);
  },

  /**
   * Sets the .iconURI property of the engine.
   *
   *  @param aIconURL
   *         A URI string pointing to the engine's icon. Must have a http[s],
   *         ftp, or data scheme. Icons with HTTP[S] or FTP schemes will be
   *         downloaded and converted to data URIs for storage in the engine
   *         XML files, if the engine is not readonly.
   *  @param aIsPreferred
   *         Whether or not this icon is to be preferred. Preferred icons can
   *         override non-preferred icons.
   */
  _setIcon: function SRCH_ENG_setIcon(aIconURL, aIsPreferred) {
    // If we already have a preferred icon, and this isn't a preferred icon,
    // just ignore it.
    if (this._hasPreferredIcon && !aIsPreferred)
      return;

    var uri = makeURI(aIconURL);

    // Ignore bad URIs
    if (!uri)
      return;

    LOG("_setIcon: Setting icon url \"" + uri.spec + "\" for engine \""
        + this.name + "\".");
    // Only accept remote icons from http[s] or ftp
    switch (uri.scheme) {
      case "data":
        this._iconURI = uri;
        notifyAction(this, SEARCH_ENGINE_CHANGED);
        this._hasPreferredIcon = aIsPreferred;
        break;
      case "http":
      case "https":
      case "ftp":
        // No use downloading the icon if the engine file is read-only
        if (!this._readOnly ||
            getBoolPref(BROWSER_SEARCH_PREF + "cache.enabled", true)) {
          LOG("_setIcon: Downloading icon: \"" + uri.spec +
              "\" for engine: \"" + this.name + "\"");
          var chan = NetUtil.ioService.newChannelFromURI(uri);

          function iconLoadCallback(aByteArray, aEngine) {
            // This callback may run after we've already set a preferred icon,
            // so check again.
            if (aEngine._hasPreferredIcon && !aIsPreferred)
              return;

            if (!aByteArray || aByteArray.length > MAX_ICON_SIZE) {
              LOG("iconLoadCallback: load failed, or the icon was too large!");
              return;
            }

            var str = btoa(String.fromCharCode.apply(null, aByteArray));
            aEngine._iconURI = makeURI(ICON_DATAURL_PREFIX + str);

            // The engine might not have a file yet, if it's being downloaded,
            // because the request for the engine file itself (_onLoad) may not
            // yet be complete. In that case, this change will be written to
            // file when _onLoad is called. For readonly engines, we'll store
            // the changes in the cache once notified below.
            if (aEngine._file && !aEngine._readOnly)
              aEngine._serializeToFile();

            notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
            aEngine._hasPreferredIcon = aIsPreferred;
          }

          // If we're currently acting as an "update engine", then the callback
          // should set the icon on the engine we're updating and not us, since
          // |this| might be gone by the time the callback runs.
          var engineToSet = this._engineToUpdate || this;

          var listener = new loadListener(chan, engineToSet, iconLoadCallback);
          chan.notificationCallbacks = listener;
          chan.asyncOpen(listener, null);
        }
        break;
    }
  },

  /**
   * Initialize this Engine object from the collected data.
   */
  _initFromData: function SRCH_ENG_initFromData() {

    ENSURE_WARN(this._data, "Can't init an engine with no data!",
                Cr.NS_ERROR_UNEXPECTED);

    // Find out what type of engine we are
    switch (this._dataType) {
      case SEARCH_DATA_XML:
        if (checkNameSpace(this._data, [MOZSEARCH_LOCALNAME],
            [MOZSEARCH_NS_10])) {

          LOG("_init: Initing MozSearch plugin from " + this._location);

          this._type = SEARCH_TYPE_MOZSEARCH;
          this._parseAsMozSearch();

        } else if (checkNameSpace(this._data, [OPENSEARCH_LOCALNAME],
                                  OPENSEARCH_NAMESPACES)) {

          LOG("_init: Initing OpenSearch plugin from " + this._location);

          this._type = SEARCH_TYPE_OPENSEARCH;
          this._parseAsOpenSearch();

        } else
          FAIL(this._location + " is not a valid search plugin.", Cr.NS_ERROR_FAILURE);

        break;
      case SEARCH_DATA_TEXT:
        LOG("_init: Initing Sherlock plugin from " + this._location);

        // the only text-based format we support is Sherlock
        this._type = SEARCH_TYPE_SHERLOCK;
        this._parseAsSherlock();
    }

    // No need to keep a ref to our data (which in some cases can be a document
    // element) past this point
    this._data = null;
  },

  /**
   * Initialize this Engine object from a collection of metadata.
   */
  _initFromMetadata: function SRCH_ENG_initMetaData(aName, aIconURL, aAlias,
                                                    aDescription, aMethod,
                                                    aTemplate) {
    ENSURE_WARN(!this._readOnly,
                "Can't call _initFromMetaData on a readonly engine!",
                Cr.NS_ERROR_FAILURE);

    this._urls.push(new EngineURL("text/html", aMethod, aTemplate));

    this._name = aName;
    this.alias = aAlias;
    this._description = aDescription;
    this._setIcon(aIconURL, true);

    this._serializeToFile();
  },

  /**
   * Extracts data from an OpenSearch URL element and creates an EngineURL
   * object which is then added to the engine's list of URLs.
   *
   * @throws NS_ERROR_FAILURE if a URL object could not be created.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag.
   * @see EngineURL()
   */
  _parseURL: function SRCH_ENG_parseURL(aElement) {
    var type     = aElement.getAttribute("type");
    // According to the spec, method is optional, defaulting to "GET" if not
    // specified
    var method   = aElement.getAttribute("method") || "GET";
    var template = aElement.getAttribute("template");

    try {
      var url = new EngineURL(type, method, template);
    } catch (ex) {
      FAIL("_parseURL: failed to add " + template + " as a URL",
           Cr.NS_ERROR_FAILURE);
    }

    if (aElement.hasAttribute("rel"))
      url.rels = aElement.getAttribute("rel").toLowerCase().split(/\s+/);

    for (var i = 0; i < aElement.childNodes.length; ++i) {
      var param = aElement.childNodes[i];
      if (param.localName == "Param") {
        try {
          url.addParam(param.getAttribute("name"), param.getAttribute("value"));
        } catch (ex) {
          // Ignore failure
          LOG("_parseURL: Url element has an invalid param");
        }
      } else if (param.localName == "MozParam" &&
                 // We only support MozParams for default search engines
                 this._isDefault) {
        var value;
        switch (param.getAttribute("condition")) {
          case "defaultEngine":
            // If this engine was the default search engine, use the true value
            if (this._isDefaultEngine())
              value = param.getAttribute("trueValue");
            else
              value = param.getAttribute("falseValue");
            url.addParam(param.getAttribute("name"), value);
            url._addMozParam({"name": param.getAttribute("name"),
                              "falseValue": param.getAttribute("falseValue"),
                              "trueValue": param.getAttribute("trueValue"),
                              "condition": "defaultEngine"});
            break;

          case "pref":
            try {
              value = getMozParamPref(param.getAttribute("pref"), value);
              url.addParam(param.getAttribute("name"), value);
              url._addMozParam({"pref": param.getAttribute("pref"),
                                "name": param.getAttribute("name"),
                                "condition": "pref"});
            } catch (e) { }
            break;
        }
      }
    }

    this._urls.push(url);
  },

  _isDefaultEngine: function SRCH_ENG__isDefaultEngine() {
    let defaultPrefB = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
    let nsIPLS = Ci.nsIPrefLocalizedString;
    let defaultEngine;
    try {
      defaultEngine = defaultPrefB.getComplexValue("defaultenginename", nsIPLS).data;
    } catch (ex) {}
    return this.name == defaultEngine;
  },

  /**
   * Get the icon from an OpenSearch Image element.
   * @see http://opensearch.a9.com/spec/1.1/description/#image
   */
  _parseImage: function SRCH_ENG_parseImage(aElement) {
    LOG("_parseImage: Image textContent: \"" + aElement.textContent + "\"");
    if (aElement.getAttribute("width")  == "16" &&
        aElement.getAttribute("height") == "16") {
      this._setIcon(aElement.textContent, true);
    }
  },

  _parseAsMozSearch: function SRCH_ENG_parseAsMoz() {
    //forward to the OpenSearch parser
    this._parseAsOpenSearch();
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parseAsOpenSearch: function SRCH_ENG_parseAsOS() {
    var doc = this._data;

    // The OpenSearch spec sets a default value for the input encoding.
    this._queryCharset = OS_PARAM_INPUT_ENCODING_DEF;

    for (var i = 0; i < doc.childNodes.length; ++i) {
      var child = doc.childNodes[i];
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
            LOG("_parseAsOpenSearch: failed to parse URL child: " + ex);
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
      }
    }
    if (!this.name || (this._urls.length == 0))
      FAIL("_parseAsOpenSearch: No name, or missing URL!", Cr.NS_ERROR_FAILURE);
    if (!this.supportsResponseType(URLTYPE_SEARCH_HTML))
      FAIL("_parseAsOpenSearch: No text/html result type!", Cr.NS_ERROR_FAILURE);
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parseAsSherlock: function SRCH_ENG_parseAsSherlock() {
    /**
     * Extracts one Sherlock "section" from aSource. A section is essentially
     * an HTML element with attributes, but each attribute must be on a new
     * line, by definition.
     *
     * @param aLines
     *        An array of lines from the sherlock file.
     * @param aSection
     *        The name of the section (e.g. "search" or "browser"). This value
     *        is not case sensitive.
     * @returns an object whose properties correspond to the section's
     *          attributes.
     */
    function getSection(aLines, aSection) {
      LOG("_parseAsSherlock::getSection: Sherlock lines:\n" +
          aLines.join("\n"));
      var lines = aLines;
      var startMark = new RegExp("^\\s*<" + aSection.toLowerCase() + "\\s*",
                                 "gi");
      var endMark   = /\s*>\s*$/gi;

      var foundStart = false;
      var startLine, numberOfLines;
      // Find the beginning and end of the section
      for (var i = 0; i < lines.length; i++) {
        if (foundStart) {
          if (endMark.test(lines[i])) {
            numberOfLines = i - startLine;
            // Remove the end marker
            lines[i] = lines[i].replace(endMark, "");
            // If the endmarker was not the only thing on the line, include
            // this line in the results
            if (lines[i])
              numberOfLines++;
            break;
          }
        } else {
          if (startMark.test(lines[i])) {
            foundStart = true;
            // Remove the start marker
            lines[i] = lines[i].replace(startMark, "");
            startLine = i;
            // If the line is empty, don't include it in the result
            if (!lines[i])
              startLine++;
          }
        }
      }
      LOG("_parseAsSherlock::getSection: Start index: " + startLine +
          "\nNumber of lines: " + numberOfLines);
      lines = lines.splice(startLine, numberOfLines);
      LOG("_parseAsSherlock::getSection: Section lines:\n" +
          lines.join("\n"));

      var section = {};
      for (var i = 0; i < lines.length; i++) {
        var line = lines[i].trim();

        var els = line.split("=");
        var name = els.shift().trim().toLowerCase();
        var value = els.join("=").trim();

        if (!name || !value)
          continue;

        // Strip leading and trailing whitespace, remove quotes from the
        // value, and remove any trailing slashes or ">" characters
        value = value.replace(/^["']/, "")
                     .replace(/["']\s*[\\\/]?>?\s*$/, "") || "";
        value = value.trim();

        // Don't clobber existing attributes
        if (!(name in section))
          section[name] = value;
      }
      return section;
    }

    /**
     * Returns an array of name-value pair arrays representing the Sherlock
     * file's input elements. User defined inputs return USER_DEFINED
     * as the value. Elements are returned in the order they appear in the
     * source file.
     *
     *   Example:
     *      <input name="foo" value="bar">
     *      <input name="foopy" user>
     *   Returns:
     *      [["foo", "bar"], ["foopy", "{searchTerms}"]]
     *
     * @param aLines
     *        An array of lines from the source file.
     */
    function getInputs(aLines) {

      /**
       * Extracts an attribute value from a given a line of text.
       *    Example: <input value="foo" name="bar">
       *      Extracts the string |foo| or |bar| given an input aAttr of
       *      |value| or |name|.
       * Attributes may be quoted or unquoted. If unquoted, any whitespace
       * indicates the end of the attribute value.
       *    Example: < value=22 33 name=44\334 >
       *      Returns |22| for "value" and |44\334| for "name".
       *
       * @param aAttr
       *        The name of the attribute for which to obtain the value. This
       *        value is not case sensitive.
       * @param aLine
       *        The line containing the attribute.
       *
       * @returns the attribute value, or an empty string if the attribute
       *          doesn't exist.
       */
      function getAttr(aAttr, aLine) {
        // Used to determine whether an "input" line from a Sherlock file is a
        // "user defined" input.
        const userInput = /(\s|["'=])user(\s|[>="'\/\\+]|$)/i;

        LOG("_parseAsSherlock::getAttr: Getting attr: \"" +
            aAttr + "\" for line: \"" + aLine + "\"");
        // We're not case sensitive, but we want to return the attribute value
        // in its original case, so create a copy of the source
        var lLine = aLine.toLowerCase();
        var attr = aAttr.toLowerCase();

        var attrStart = lLine.search(new RegExp("\\s" + attr, "i"));
        if (attrStart == -1) {

          // If this is the "user defined input" (i.e. contains the empty
          // "user" attribute), return our special keyword
          if (userInput.test(lLine) && attr == "value") {
            LOG("_parseAsSherlock::getAttr: Found user input!\nLine:\"" + lLine
                + "\"");
            return USER_DEFINED;
          }
          // The attribute doesn't exist - ignore
          LOG("_parseAsSherlock::getAttr: Failed to find attribute:\nLine:\""
              + lLine + "\"\nAttr:\"" + attr + "\"");
          return "";
        }

        var valueStart = lLine.indexOf("=", attrStart) + "=".length;
        if (valueStart == -1)
          return "";

        var quoteStart = lLine.indexOf("\"", valueStart);
        if (quoteStart == -1) {

          // Unquoted attribute, get the rest of the line, trimmed at the first
          // sign of whitespace. If the rest of the line is only whitespace,
          // returns a blank string.
          return lLine.substr(valueStart).replace(/\s.*$/, "");

        } else {
          // Make sure that there's only whitespace between the start of the
          // value and the first quote. If there is, end the attribute value at
          // the first sign of whitespace. This prevents us from falling into
          // the next attribute if this is an unquoted attribute followed by a
          // quoted attribute.
          var betweenEqualAndQuote = lLine.substring(valueStart, quoteStart);
          if (/\S/.test(betweenEqualAndQuote))
            return lLine.substr(valueStart).replace(/\s.*$/, "");

          // Adjust the start index to account for the opening quote
          valueStart = quoteStart + "\"".length;
          // Find the closing quote
          var valueEnd = lLine.indexOf("\"", valueStart);
          // If there is no closing quote, just go to the end of the line
          if (valueEnd == -1)
            valueEnd = aLine.length;
        }
        return aLine.substring(valueStart, valueEnd);
      }

      var inputs = [];

      LOG("_parseAsSherlock::getInputs: Lines:\n" + aLines);
      // Filter out everything but non-inputs
      let lines = aLines.filter(function (line) {
        return /^\s*<input/i.test(line);
      });
      LOG("_parseAsSherlock::getInputs: Filtered lines:\n" + lines);

      lines.forEach(function (line) {
        // Strip leading/trailing whitespace and remove the surrounding markup
        // ("<input" and ">")
        line = line.trim().replace(/^<input/i, "").replace(/>$/, "");

        // If this is one of the "directional" inputs (<inputnext>/<inputprev>)
        const directionalInput = /^(prev|next)/i;
        if (directionalInput.test(line)) {

          // Make it look like a normal input by removing "prev" or "next"
          line = line.replace(directionalInput, "");

          // If it has a name, give it a dummy value to match previous
          // nsInternetSearchService behavior
          if (/name\s*=/i.test(line)) {
            line += " value=\"0\"";
          } else
            return; // Line has no name, skip it
        }

        var attrName = getAttr("name", line);
        var attrValue = getAttr("value", line);
        LOG("_parseAsSherlock::getInputs: Got input:\nName:\"" + attrName +
            "\"\nValue:\"" + attrValue + "\"");
        if (attrValue)
          inputs.push([attrName, attrValue]);
      });
      return inputs;
    }

    function err(aErr) {
      FAIL("_parseAsSherlock::err: Sherlock param error:\n" + aErr,
           Cr.NS_ERROR_FAILURE);
    }

    // First try converting our byte array using the default Sherlock encoding.
    // If this fails, or if we find a sourceTextEncoding attribute, we need to
    // reconvert the byte array using the specified encoding.
    var sherlockLines, searchSection, sourceTextEncoding, browserSection;
    try {
      sherlockLines = sherlockBytesToLines(this._data);
      searchSection = getSection(sherlockLines, "search");
      browserSection = getSection(sherlockLines, "browser");
      sourceTextEncoding = parseInt(searchSection["sourcetextencoding"]);
      if (sourceTextEncoding) {
        // Re-convert the bytes using the found sourceTextEncoding
        sherlockLines = sherlockBytesToLines(this._data, sourceTextEncoding);
        searchSection = getSection(sherlockLines, "search");
        browserSection = getSection(sherlockLines, "browser");
      }
    } catch (ex) {
      // The conversion using the default charset failed. Remove any non-ascii
      // bytes and try to find a sourceTextEncoding.
      var asciiBytes = this._data.filter(function (n) {return !(0x80 & n);});
      var asciiString = String.fromCharCode.apply(null, asciiBytes);
      sherlockLines = asciiString.split(NEW_LINES).filter(isUsefulLine);
      searchSection = getSection(sherlockLines, "search");
      sourceTextEncoding = parseInt(searchSection["sourcetextencoding"]);
      if (sourceTextEncoding) {
        sherlockLines = sherlockBytesToLines(this._data, sourceTextEncoding);
        searchSection = getSection(sherlockLines, "search");
        browserSection = getSection(sherlockLines, "browser");
      } else
        ERROR("Couldn't find a working charset", Cr.NS_ERROR_FAILURE);
    }

    LOG("_parseAsSherlock: Search section:\n" + searchSection.toSource());

    this._name = searchSection["name"] || err("Missing name!");
    this._description = searchSection["description"] || "";
    this._queryCharset = searchSection["querycharset"] ||
                         queryCharsetFromCode(searchSection["queryencoding"]);
    this._searchForm = searchSection["searchform"];

    this._updateInterval = parseInt(browserSection["updatecheckdays"]);

    this._updateURL = browserSection["update"];
    this._iconUpdateURL = browserSection["updateicon"];

    var method = (searchSection["method"] || "GET").toUpperCase();
    var template = searchSection["action"] || err("Missing action!");

    var inputs = getInputs(sherlockLines);
    LOG("_parseAsSherlock: Inputs:\n" + inputs.toSource());

    var url = null;

    if (method == "GET") {
      // Here's how we construct the input string:
      // <input> is first:  Name Attr:  Prefix      Data           Example:
      // YES                EMPTY       None        <value>        TEMPLATE<value>
      // YES                NON-EMPTY   ?           <name>=<value> TEMPLATE?<name>=<value>
      // NO                 EMPTY       ------------- <ignored> --------------
      // NO                 NON-EMPTY   &           <name>=<value> TEMPLATE?<n1>=<v1>&<n2>=<v2>
      for (var i = 0; i < inputs.length; i++) {
        var name  = inputs[i][0];
        var value = inputs[i][1];
        if (i==0) {
          if (name == "")
            template += USER_DEFINED;
          else
            template += "?" + name + "=" + value;
        } else if (name != "")
          template += "&" + name + "=" + value;
      }
      url = new EngineURL("text/html", method, template);

    } else if (method == "POST") {
      // Create the URL object and just add the parameters directly
      url = new EngineURL("text/html", method, template);
      for (var i = 0; i < inputs.length; i++) {
        var name  = inputs[i][0];
        var value = inputs[i][1];
        if (name)
          url.addParam(name, value);
      }
    } else
      err("Invalid method!");

    this._urls.push(url);
  },

  /**
   * Init from a JSON record.
   **/
  _initWithJSON: function SRCH_ENG__initWithJSON(aJson) {
    this.__id = aJson._id;
    this._name = aJson._name;
    this._description = aJson.description;
    if (aJson._hasPreferredIcon == undefined)
      this._hasPreferredIcon = true;
    else
      this._hasPreferredIcon = false;
    this._hidden = aJson._hidden;
    this._type = aJson.type || SEARCH_TYPE_MOZSEARCH;
    this._queryCharset = aJson.queryCharset || DEFAULT_QUERY_CHARSET;
    this.__searchForm = aJson.__searchForm;
    this.__installLocation = aJson._installLocation || SEARCH_APP_DIR;
    this._updateInterval = aJson._updateInterval || null;
    this._updateURL = aJson._updateURL || null;
    this._iconUpdateURL = aJson._iconUpdateURL || null;
    if (aJson._readOnly == undefined)
      this._readOnly = true;
    else
      this._readOnly = false;
    this._iconURI = makeURI(aJson._iconURL);
    for (let i = 0; i < aJson._urls.length; ++i) {
      let url = aJson._urls[i];
      let engineURL = new EngineURL(url.type || URLTYPE_SEARCH_HTML,
                                    url.method || "GET", url.template);
      engineURL._initWithJSON(url, this);
      this._urls.push(engineURL);
    }
  },

  /**
   * Creates a JavaScript object that represents this engine.
   * @param aFilter
   *        Whether or not to filter out common default values. Recommended for
   *        use with _initWithJSON().
   * @returns An object suitable for serialization as JSON.
   **/
  _serializeToJSON: function SRCH_ENG__serializeToJSON(aFilter) {
    var json = {
      _id: this._id,
      _name: this._name,
      _hidden: this.hidden,
      description: this.description,
      __searchForm: this.__searchForm,
      _iconURL: this._iconURL,
      _urls: [url._serializeToJSON() for each(url in this._urls)] 
    };

    if (this._file instanceof Ci.nsILocalFile)
      json.filePath = this._file.persistentDescriptor;
    if (this._uri)
      json._url = this._uri.spec;
    if (this._installLocation != SEARCH_APP_DIR || !aFilter)
      json._installLocation = this._installLocation;
    if (this._updateInterval || !aFilter)
      json._updateInterval = this._updateInterval;
    if (this._updateURL || !aFilter)
      json._updateURL = this._updateURL;
    if (this._iconUpdateURL || !aFilter)
      json._iconUpdateURL = this._iconUpdateURL;
    if (!this._hasPreferredIcon || !aFilter)
      json._hasPreferredIcon = this._hasPreferredIcon;
    if (this.type != SEARCH_TYPE_MOZSEARCH || !aFilter)
      json.type = this.type;
    if (this.queryCharset != DEFAULT_QUERY_CHARSET || !aFilter)
      json.queryCharset = this.queryCharset;
    if (this._dataType != SEARCH_DATA_XML || !aFilter)
      json._dataType = this._dataType;
    if (!this._readOnly || !aFilter)
      json._readOnly = this._readOnly;

    return json;
  },

  /**
   * Returns an XML document object containing the search plugin information,
   * which can later be used to reload the engine.
   */
  _serializeToElement: function SRCH_ENG_serializeToEl() {
    function appendTextNode(aNameSpace, aLocalName, aValue) {
      if (!aValue)
        return null;
      var node = doc.createElementNS(aNameSpace, aLocalName);
      node.appendChild(doc.createTextNode(aValue));
      docElem.appendChild(node);
      docElem.appendChild(doc.createTextNode("\n"));
      return node;
    }

    var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);

    var doc = parser.parseFromString(EMPTY_DOC, "text/xml");
    var docElem = doc.documentElement;

    docElem.appendChild(doc.createTextNode("\n"));

    appendTextNode(OPENSEARCH_NS_11, "ShortName", this.name);
    appendTextNode(OPENSEARCH_NS_11, "Description", this._description);
    appendTextNode(OPENSEARCH_NS_11, "InputEncoding", this._queryCharset);

    if (this._iconURI) {
      var imageNode = appendTextNode(OPENSEARCH_NS_11, "Image",
                                     this._iconURI.spec);
      if (imageNode) {
        imageNode.setAttribute("width", "16");
        imageNode.setAttribute("height", "16");
      }
    }

    appendTextNode(MOZSEARCH_NS_10, "UpdateInterval", this._updateInterval);
    appendTextNode(MOZSEARCH_NS_10, "UpdateUrl", this._updateURL);
    appendTextNode(MOZSEARCH_NS_10, "IconUpdateUrl", this._iconUpdateURL);
    appendTextNode(MOZSEARCH_NS_10, "SearchForm", this._searchForm);

    for (var i = 0; i < this._urls.length; ++i)
      this._urls[i]._serializeToElement(doc, docElem);
    docElem.appendChild(doc.createTextNode("\n"));

    return doc;
  },

  _lazySerializeToFile: function SRCH_ENG_serializeToFile() {
    if (this._serializeTimer) {
      // Reset the timer
      this._serializeTimer.delay = LAZY_SERIALIZE_DELAY;
    } else {
      this._serializeTimer = Cc["@mozilla.org/timer;1"].
                             createInstance(Ci.nsITimer);
      var timerCallback = {
        self: this,
        notify: function SRCH_ENG_notify(aTimer) {
          try {
            this.self._serializeToFile();
          } catch (ex) {
            LOG("Serialization from timer callback failed:\n" + ex);
          }
          this.self._serializeTimer = null;
        }
      };
      this._serializeTimer.initWithCallback(timerCallback,
                                            LAZY_SERIALIZE_DELAY,
                                            Ci.nsITimer.TYPE_ONE_SHOT);
    }
  },

  /**
   * Serializes the engine object to file.
   */
  _serializeToFile: function SRCH_ENG_serializeToFile() {
    var file = this._file;
    ENSURE_WARN(!this._readOnly, "Can't serialize a read only engine!",
                Cr.NS_ERROR_FAILURE);
    ENSURE_WARN(file && file.exists(), "Can't serialize: file doesn't exist!",
                Cr.NS_ERROR_UNEXPECTED);

    var fos = Cc["@mozilla.org/network/safe-file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);

    // Serialize the engine first - we don't want to overwrite a good file
    // if this somehow fails.
    var doc = this._serializeToElement();

    fos.init(file, (MODE_WRONLY | MODE_TRUNCATE), PERMS_FILE, 0);

    try {
      var serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                       createInstance(Ci.nsIDOMSerializer);
      serializer.serializeToStream(doc.documentElement, fos, null);
    } catch (e) {
      LOG("_serializeToFile: Error serializing engine:\n" + e);
    }

    closeSafeOutputStream(fos);
  },

  /**
   * Remove the engine's file from disk. The search service calls this once it
   * removes the engine from its internal store. This function will throw if
   * the file cannot be removed.
   */
  _remove: function SRCH_ENG_remove() {
    if (this._readOnly)
      FAIL("Can't remove read only engine!", Cr.NS_ERROR_FAILURE);
    if (!this._file || !this._file.exists())
      FAIL("Can't remove engine: file doesn't exist!", Cr.NS_ERROR_FILE_NOT_FOUND);

    this._file.remove(false);
  },

  // nsISearchEngine
  get alias() {
    if (this._alias === undefined)
      this._alias = engineMetadataService.getAttr(this, "alias");

    return this._alias;
  },
  set alias(val) {
    this._alias = val;
    engineMetadataService.setAttr(this, "alias", val);
    notifyAction(this, SEARCH_ENGINE_CHANGED);
  },

  get description() {
    return this._description;
  },

  get hidden() {
    if (this._hidden === null)
      this._hidden = engineMetadataService.getAttr(this, "hidden") || false;
    return this._hidden;
  },
  set hidden(val) {
    var value = !!val;
    if (value != this._hidden) {
      this._hidden = value;
      engineMetadataService.setAttr(this, "hidden", value);
      notifyAction(this, SEARCH_ENGINE_CHANGED);
    }
  },

  get iconURI() {
    return this._iconURI;
  },

  get _iconURL() {
    if (!this._iconURI)
      return "";
    return this._iconURI.spec;
  },

  // Where the engine is being loaded from: will return the URI's spec if the
  // engine is being downloaded and does not yet have a file. This is only used
  // for logging and error messages.
  get _location() {
    if (this._file)
      return this._file.path;

    if (this._uri)
      return this._uri.spec;

    return "";
  },

  // The file that the plugin is loaded from is a unique identifier for it.  We
  // use this as the identifier to store data in the sqlite database
  __id: null,
  get _id() {
    if (this.__id)
      return this.__id;

    // Treat engines loaded from JARs the same way we treat app shipped
    // engines.
    // Theoretically, these could also come from extensions, but there's no
    // real way for extensions to register their chrome locations at the
    // moment, so let's not deal with that case.
    // This means we're vulnerable to conflicts if a file loaded from a JAR
    // has the same filename as a file loaded from the app dir, but with a
    // different engine name. People using the JAR functionality should be
    // careful not to do that!
    if (this._isInAppDir || this._isInJAR) {
      let leafName;
      if (this._file)
        leafName = this._file.leafName;
      else {
        // If we've reached this point, we must be loaded from a JAR, which
        // also means we should have a URL.
        ENSURE_WARN(this._isInJAR && (this._uri instanceof Ci.nsIURL),
                    "_id: not inJAR, or no URI", Cr.NS_ERROR_UNEXPECTED);
        leafName = this._uri.fileName;
      }

      return this.__id = "[app]/" + leafName;
    }

    ENSURE_WARN(this._file, "_id: no _file!", Cr.NS_ERROR_UNEXPECTED);

    if (this._isInProfile)
      return this.__id = "[profile]/" + this._file.leafName;

    // We're not in the profile or appdir, so this must be an extension-shipped
    // plugin. Use the full filename.
    return this.__id  = this._file.path;
  },

  get _installLocation() {
    if (this.__installLocation === null) {
      if (!this._file) {
        ENSURE_WARN(this._uri, "Engines without files must have URIs",
                    Cr.NS_ERROR_UNEXPECTED);
        this.__installLocation = SEARCH_JAR;
      }
      else if (this._file.parent.equals(getDir(NS_APP_SEARCH_DIR)))
        this.__installLocation = SEARCH_APP_DIR;
      else if (this._file.parent.equals(getDir(NS_APP_USER_SEARCH_DIR)))
        this.__installLocation = SEARCH_PROFILE_DIR;
      else
        this.__installLocation = SEARCH_IN_EXTENSION;
    }

    return this.__installLocation;
  },

  get _isInJAR() {
    return this._installLocation == SEARCH_JAR;
  },
  get _isInAppDir() {
    return this._installLocation == SEARCH_APP_DIR;
  },
  get _isInProfile() {
    return this._installLocation == SEARCH_PROFILE_DIR;
  },

  get _isDefault() {
    // For now, our concept of a "default engine" is "one that is not in the
    // user's profile directory", which is currently equivalent to "is app- or
    // extension-shipped".
    return !this._isInProfile;
  },

  get _hasUpdates() {
    // Whether or not the engine has an update URL
    let selfURL = this._getURLOfType(URLTYPE_OPENSEARCH);
    return !!(this._updateURL || this._iconUpdateURL || (selfURL &&
              selfURL._hasRelation("self")));
  },

  get name() {
    return this._name;
  },

  get type() {
    return this._type;
  },

  get searchForm() {
    if (!this._searchForm) {
      // No searchForm specified in the engine definition file, use the prePath
      // (e.g. https://foo.com for https://foo.com/search.php?q=bar).
      var htmlUrl = this._getURLOfType(URLTYPE_SEARCH_HTML);
      ENSURE_WARN(htmlUrl, "Engine has no HTML URL!", Cr.NS_ERROR_UNEXPECTED);
      this._searchForm = makeURI(htmlUrl.template).prePath;
    }

    return ParamSubstitution(this._searchForm, "", this);
  },

  get queryCharset() {
    if (this._queryCharset)
      return this._queryCharset;
    return this._queryCharset = queryCharsetFromCode(/* get the default */);
  },

  // from nsISearchEngine
  addParam: function SRCH_ENG_addParam(aName, aValue, aResponseType) {
    if (!aName || (aValue == null))
      FAIL("missing name or value for nsISearchEngine::addParam!");
    ENSURE_WARN(!this._readOnly,
                "called nsISearchEngine::addParam on a read-only engine!",
                Cr.NS_ERROR_FAILURE);
    if (!aResponseType)
      aResponseType = URLTYPE_SEARCH_HTML;

    var url = this._getURLOfType(aResponseType);
    if (!url)
      FAIL("Engine object has no URL for response type " + aResponseType,
           Cr.NS_ERROR_FAILURE);

    url.addParam(aName, aValue);

    // Serialize the changes to file lazily
    this._lazySerializeToFile();
  },

  // from nsISearchEngine
  getSubmission: function SRCH_ENG_getSubmission(aData, aResponseType) {
    if (!aResponseType)
      aResponseType = URLTYPE_SEARCH_HTML;

    // Check for updates on the first use of an app-shipped engine
    if (this._isInAppDir && aResponseType == URLTYPE_SEARCH_HTML && !this._used) {
      this._used = true;
      engineUpdateService.update(this);
    }

    var url = this._getURLOfType(aResponseType);

    if (!url)
      return null;

    if (!aData) {
      // Return a dummy submission object with our searchForm attribute
      return new Submission(makeURI(this.searchForm), null);
    }

    LOG("getSubmission: In data: \"" + aData + "\"");
    var textToSubURI = Cc["@mozilla.org/intl/texttosuburi;1"].
                       getService(Ci.nsITextToSubURI);
    var data = "";
    try {
      data = textToSubURI.ConvertAndEscape(this.queryCharset, aData);
    } catch (ex) {
      LOG("getSubmission: Falling back to default queryCharset!");
      data = textToSubURI.ConvertAndEscape(DEFAULT_QUERY_CHARSET, aData);
    }
    LOG("getSubmission: Out data: \"" + data + "\"");
    return url.getSubmission(data, this);
  },

  // from nsISearchEngine
  supportsResponseType: function SRCH_ENG_supportsResponseType(type) {
    return (this._getURLOfType(type) != null);
  },

  // nsISupports
  QueryInterface: function SRCH_ENG_QI(aIID) {
    if (aIID.equals(Ci.nsISearchEngine) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  get wrappedJSObject() {
    return this;
  }

};

// nsISearchSubmission
function Submission(aURI, aPostData) {
  this._uri = aURI;
  this._postData = aPostData;
}
Submission.prototype = {
  get uri() {
    return this._uri;
  },
  get postData() {
    return this._postData;
  },
  QueryInterface: function SRCH_SUBM_QI(aIID) {
    if (aIID.equals(Ci.nsISearchSubmission) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

// nsIBrowserSearchService
function SearchService() {
  // Replace empty LOG function with the useful one if the log pref is set.
  if (getBoolPref(BROWSER_SEARCH_PREF + "log", false))
    LOG = DO_LOG;

  try {
    this._loadEngines();
  } catch (ex) {
    LOG("_init: failure loading engines: " + ex);
  }
  gEnginesLoaded = true;
  this._addObservers();
}
SearchService.prototype = {
  classID: Components.ID("{7319788a-fe93-4db3-9f39-818cf08f4256}"),

  _engines: { },
  __sortedEngines: null,
  get _sortedEngines() {
    if (!this.__sortedEngines)
      return this._buildSortedEngineList();
    return this.__sortedEngines;
  },

  // Whether or not we need to write the order of engines on shutdown. This
  // needs to happen anytime _sortedEngines is modified after initial startup. 
  _needToSetOrderPrefs: false,

  _buildCache: function SRCH_SVC__buildCache() {
    if (!getBoolPref(BROWSER_SEARCH_PREF + "cache.enabled", true))
      return;

    let cache = {};
    let locale = getLocale();
    let buildID = Services.appinfo.platformBuildID;

    // Allows us to force a cache refresh should the cache format change.
    cache.version = CACHE_VERSION;
    // We don't want to incur the costs of stat()ing each plugin on every
    // startup when the only (supported) time they will change is during
    // runtime (where we refresh for changes through the API) and app updates
    // (where the buildID is obviously going to change).
    // Extension-shipped plugins are the only exception to this, but their
    // directories are blown away during updates, so we'll detect their changes.
    cache.buildID = buildID;
    cache.locale = locale;

    cache.directories = {};

    function getParent(engine) {
      if (engine._file)
        return engine._file.parent;

      let uri = engine._uri;
      if (!uri.schemeIs("chrome")) {
        LOG("getParent: engine URI must be a chrome URI if it has no file");
        return null;
      }

      // use the underlying JAR file, for chrome URIs
      try {
        uri = gChromeReg.convertChromeURL(uri);
        if (uri instanceof Ci.nsINestedURI)
          uri = uri.innermostURI;
        uri.QueryInterface(Ci.nsIFileURL)

        return uri.file;
      } catch (ex) {
        LOG("getParent: couldn't map chrome:// URI to a file: " + ex)
      }

      return null;
    }

    for each (let engine in this._engines) {
      let parent = getParent(engine);
      if (!parent) {
        LOG("Error: no parent for engine " + engine._location + ", failing to cache it");

        continue;
      }

      let cacheKey = parent.path;
      if (!cache.directories[cacheKey]) {
        let cacheEntry = {};
        cacheEntry.lastModifiedTime = parent.lastModifiedTime;
        cacheEntry.engines = [];
        cache.directories[cacheKey] = cacheEntry;
      }
      cache.directories[cacheKey].engines.push(engine._serializeToJSON(true));
    }

    let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                  createInstance(Ci.nsIFileOutputStream);
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    let cacheFile = getDir(NS_APP_USER_PROFILE_50_DIR);
    cacheFile.append("search.json");

    try {
      LOG("_buildCache: Writing to cache file.");
      ostream.init(cacheFile, (MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE), PERMS_FILE, ostream.DEFER_OPEN);
      converter.charset = "UTF-8";
      let data = converter.convertToInputStream(JSON.stringify(cache));

      // Write to the cache file asynchronously
      NetUtil.asyncCopy(data, ostream, function(rv) {
        if (!Components.isSuccessCode(rv))
          LOG("_buildCache: failure during asyncCopy: " + rv);
      });
    } catch (ex) {
      LOG("_buildCache: Could not write to cache file: " + ex);
    }
  },

  _loadEngines: function SRCH_SVC__loadEngines() {
    LOG("_loadEngines: start");
    // See if we have a cache file so we don't have to parse a bunch of XML.
    let cache = {};
    let cacheEnabled = getBoolPref(BROWSER_SEARCH_PREF + "cache.enabled", true);
    if (cacheEnabled) {
      let cacheFile = getDir(NS_APP_USER_PROFILE_50_DIR);
      cacheFile.append("search.json");
      if (cacheFile.exists())
        cache = this._readCacheFile(cacheFile);
    }

    let loadDirs = [];
    let locations = getDir(NS_APP_SEARCH_DIR_LIST, Ci.nsISimpleEnumerator);
    while (locations.hasMoreElements()) {
      let dir = locations.getNext().QueryInterface(Ci.nsIFile);
      if (dir.directoryEntries.hasMoreElements())
        loadDirs.push(dir);
    }

    let loadFromJARs = getBoolPref(BROWSER_SEARCH_PREF + "loadFromJars", false);
    let chromeURIs = [];
    let chromeFiles = [];
    if (loadFromJARs)
      [chromeFiles, chromeURIs] = this._findJAREngines();

    let toLoad = chromeFiles.concat(loadDirs);

    function modifiedDir(aDir) {
      return (!cache.directories[aDir.path] ||
              cache.directories[aDir.path].lastModifiedTime != aDir.lastModifiedTime);
    }

    function notInToLoad(aCachePath, aIndex)
      aCachePath != toLoad[aIndex].path;

    let buildID = Services.appinfo.platformBuildID;
    let cachePaths = [path for (path in cache.directories)];

    let rebuildCache = !cache.directories ||
                       cache.version != CACHE_VERSION ||
                       cache.locale != getLocale() ||
                       cache.buildID != buildID ||
                       cachePaths.length != toLoad.length ||
                       cachePaths.some(notInToLoad) ||
                       toLoad.some(modifiedDir);

    if (!cacheEnabled || rebuildCache) {
      LOG("_loadEngines: Absent or outdated cache. Loading engines from disk.");
      loadDirs.forEach(this._loadEnginesFromDir, this);

      this._loadFromChromeURLs(chromeURIs);

      if (cacheEnabled)
        this._buildCache();
      return;
    }

    LOG("_loadEngines: loading from cache directories");
    for each (let dir in cache.directories)
      this._loadEnginesFromCache(dir);

    LOG("_loadEngines: done");
  },

  _readCacheFile: function SRCH_SVC__readCacheFile(aFile) {
    let stream = Cc["@mozilla.org/network/file-input-stream;1"].
                 createInstance(Ci.nsIFileInputStream);
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);

    try {
      stream.init(aFile, MODE_RDONLY, PERMS_FILE, 0);
      return json.decodeFromStream(stream, stream.available());
    } catch(ex) {
      LOG("_readCacheFile: Error reading cache file: " + ex);
    } finally {
      stream.close();
    }
    return false;
  },

  _batchTimer: null,
  _batchCacheInvalidation: function SRCH_SVC__batchCacheInvalidation() {
    let callback = {
      self: this,
      notify: function SRCH_SVC_batchTimerNotify(aTimer) {
        LOG("_batchCacheInvalidation: Invalidating engine cache");
        this.self._buildCache();
        this.self._batchTimer = null;
      }
    };

    if (!this._batchTimer) {
      this._batchTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._batchTimer.initWithCallback(callback, CACHE_INVALIDATION_DELAY,
                                        Ci.nsITimer.TYPE_ONE_SHOT);
    } else {
      this._batchTimer.delay = CACHE_INVALIDATION_DELAY;
      LOG("_batchCacheInvalidation: Batch timer reset");
    }
  },

  _addEngineToStore: function SRCH_SVC_addEngineToStore(aEngine) {
    LOG("_addEngineToStore: Adding engine: \"" + aEngine.name + "\"");

    // See if there is an existing engine with the same name. However, if this
    // engine is updating another engine, it's allowed to have the same name.
    var hasSameNameAsUpdate = (aEngine._engineToUpdate &&
                               aEngine.name == aEngine._engineToUpdate.name);
    if (aEngine.name in this._engines && !hasSameNameAsUpdate) {
      LOG("_addEngineToStore: Duplicate engine found, aborting!");
      return;
    }

    if (aEngine._engineToUpdate) {
      // We need to replace engineToUpdate with the engine that just loaded.
      var oldEngine = aEngine._engineToUpdate;

      // Remove the old engine from the hash, since it's keyed by name, and our
      // name might change (the update might have a new name).
      delete this._engines[oldEngine.name];

      // Hack: we want to replace the old engine with the new one, but since
      // people may be holding refs to the nsISearchEngine objects themselves,
      // we'll just copy over all "private" properties (those without a getter
      // or setter) from one object to the other.
      for (var p in aEngine) {
        if (!(aEngine.__lookupGetter__(p) || aEngine.__lookupSetter__(p)))
          oldEngine[p] = aEngine[p];
      }
      aEngine = oldEngine;
      aEngine._engineToUpdate = null;

      // Add the engine back
      this._engines[aEngine.name] = aEngine;
      notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
    } else {
      // Not an update, just add the new engine.
      this._engines[aEngine.name] = aEngine;
      // Only add the engine to the list of sorted engines if the initial list
      // has already been built (i.e. if this.__sortedEngines is non-null). If
      // it hasn't, we're loading engines from disk and the sorted engine list
      // will be built once we need it.
      if (this.__sortedEngines) {
        this.__sortedEngines.push(aEngine);
        this._needToSetOrderPrefs = true;
      }
      notifyAction(aEngine, SEARCH_ENGINE_ADDED);
    }

    if (aEngine._hasUpdates) {
      // Schedule the engine's next update, if it isn't already.
      if (!engineMetadataService.getAttr(aEngine, "updateexpir"))
        engineUpdateService.scheduleNextUpdate(aEngine);
  
      // We need to save the engine's _dataType, if this is the first time the
      // engine is added to the dataStore, since ._dataType isn't persisted
      // and will change on the next startup (since the engine will then be
      // XML). We need this so that we know how to load any future updates from
      // this engine.
      if (!engineMetadataService.getAttr(aEngine, "updatedatatype"))
        engineMetadataService.setAttr(aEngine, "updatedatatype",
                                      aEngine._dataType);
    }
  },

  _loadEnginesFromCache: function SRCH_SVC__loadEnginesFromCache(aDir) {
    let engines = aDir.engines;
    LOG("_loadEnginesFromCache: Loading from cache. " + engines.length + " engines to load.");
    for (let i = 0; i < engines.length; i++) {
      let json = engines[i];

      try {
        let engine;
        if (json.filePath)
          engine = new Engine({type: "filePath", value: json.filePath}, json._dataType,
                               json._readOnly);
        else if (json._url)
          engine = new Engine({type: "uri", value: json._url}, json._dataType, json._readOnly);

        engine._initWithJSON(json);
        this._addEngineToStore(engine);
      } catch (ex) {
        LOG("Failed to load " + engines[i]._name + " from cache: " + ex);
        LOG("Engine JSON: " + engines[i].toSource());
      }
    }
  },

  _loadEnginesFromDir: function SRCH_SVC__loadEnginesFromDir(aDir) {
    LOG("_loadEnginesFromDir: Searching in " + aDir.path + " for search engines.");

    // Check whether aDir is the user profile dir
    var isInProfile = aDir.equals(getDir(NS_APP_USER_SEARCH_DIR));

    var files = aDir.directoryEntries
                    .QueryInterface(Ci.nsIDirectoryEnumerator);

    while (files.hasMoreElements()) {
      var file = files.nextFile;

      // Ignore hidden and empty files, and directories
      if (!file.isFile() || file.fileSize == 0 || file.isHidden())
        continue;

      var fileURL = NetUtil.ioService.newFileURI(file).QueryInterface(Ci.nsIURL);
      var fileExtension = fileURL.fileExtension.toLowerCase();
      var isWritable = isInProfile && file.isWritable();

      var dataType;
      switch (fileExtension) {
        case XML_FILE_EXT:
          dataType = SEARCH_DATA_XML;
          break;
        case SHERLOCK_FILE_EXT:
          dataType = SEARCH_DATA_TEXT;
          break;
        default:
          // Not an engine
          continue;
      }

      var addedEngine = null;
      try {
        addedEngine = new Engine(file, dataType, !isWritable);
        addedEngine._initFromFile();
        if (addedEngine._used)
          addedEngine._used = false;
      } catch (ex) {
        LOG("_loadEnginesFromDir: Failed to load " + file.path + "!\n" + ex);
        continue;
      }

      if (fileExtension == SHERLOCK_FILE_EXT) {
        if (isWritable) {
          try {
            this._convertSherlockFile(addedEngine, fileURL.fileBaseName);
          } catch (ex) {
            LOG("_loadEnginesFromDir: Failed to convert: " + fileURL.path + "\n" + ex + "\n" + ex.stack);
            // The engine couldn't be converted, mark it as read-only
            addedEngine._readOnly = true;
          }
        }

        // If the engine still doesn't have an icon, see if we can find one
        if (!addedEngine._iconURI) {
          var icon = this._findSherlockIcon(file, fileURL.fileBaseName);
          if (icon)
            addedEngine._iconURI = NetUtil.ioService.newFileURI(icon);
        }
      }

      this._addEngineToStore(addedEngine);
    }
  },

  _loadFromChromeURLs: function SRCH_SVC_loadFromChromeURLs(aURLs) {
    aURLs.forEach(function (url) {
      try {
        LOG("_loadFromChromeURLs: loading engine from chrome url: " + url);

        let engine = new Engine(makeURI(url), SEARCH_DATA_XML, true);
  
        engine._initFromURISync();
  
        this._addEngineToStore(engine);
      } catch (ex) {
        LOG("_loadFromChromeURLs: failed to load engine: " + ex);
      }
    }, this);
  },

  _findJAREngines: function SRCH_SVC_findJAREngines() {
    LOG("_findJAREngines: looking for engines in JARs")

    let rootURIPref = ""
    try {
      rootURIPref = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "jarURIs");
    } catch (ex) {}

    if (!rootURIPref) {
      LOG("_findJAREngines: no JAR URIs were specified");

      return [[], []];
    }

    let rootURIs = rootURIPref.split(",");
    let uris = [];
    let chromeFiles = [];

    rootURIs.forEach(function (root) {
      // Find the underlying JAR file for this chrome package (_loadEngines uses
      // it to determine whether it needs to invalidate the cache)
      let chromeFile;
      try {
        let chromeURI = gChromeReg.convertChromeURL(makeURI(root));
        let fileURI = chromeURI; // flat packaging
        while (fileURI instanceof Ci.nsIJARURI)
          fileURI = fileURI.JARFile; // JAR packaging
        fileURI.QueryInterface(Ci.nsIFileURL);
        chromeFile = fileURI.file;
      } catch (ex) {
        LOG("_findJAREngines: failed to get chromeFile for " + root + ": " + ex);
      }

      if (!chromeFile)
        return;

      chromeFiles.push(chromeFile);

      // Read list.txt from the chrome package to find the engines we need to
      // load
      let listURL = root + "list.txt";
      let names = [];
      try {
        let chan = NetUtil.ioService.newChannelFromURI(makeURI(listURL));
        let sis = Cc["@mozilla.org/scriptableinputstream;1"].
                  createInstance(Ci.nsIScriptableInputStream);
        sis.init(chan.open());
        let list = sis.read(sis.available());
        names = list.split("\n").filter(function (n) !!n);
      } catch (ex) {
        LOG("_findJAREngines: failed to retrieve list.txt from " + listURL + ": " + ex);

        return;
      }

      names.forEach(function (n) uris.push(root + n + ".xml"));
    });
    
    return [chromeFiles, uris];
  },

  _saveSortedEngineList: function SRCH_SVC_saveSortedEngineList() {
    // We only need to write the prefs. if something has changed.
    LOG("SRCH_SVC_saveSortedEngineList: starting");
    if (!this._needToSetOrderPrefs)
      return;

    LOG("SRCH_SVC_saveSortedEngineList: something to do");

    // Set the useDB pref to indicate that from now on we should use the order
    // information stored in the database.
    Services.prefs.setBoolPref(BROWSER_SEARCH_PREF + "useDBForOrder", true);

    var engines = this._getSortedEngines(true);

    let instructions = [];
    for (var i = 0; i < engines.length; ++i) {
      instructions.push(
        {key: "order",
         value: i+1,
         engine: engines[i]
        });
    }

    engineMetadataService.setAttrs(instructions);
    LOG("SRCH_SVC_saveSortedEngineList: done");
  },

  _buildSortedEngineList: function SRCH_SVC_buildSortedEngineList() {
    LOG("_buildSortedEngineList: building list");
    var addedEngines = { };
    this.__sortedEngines = [];
    var engine;

    // If the user has specified a custom engine order, read the order
    // information from the engineMetadataService instead of the default
    // prefs.
    if (getBoolPref(BROWSER_SEARCH_PREF + "useDBForOrder", false)) {
      LOG("_buildSortedEngineList: using db for order");
      for each (engine in this._engines) {
        var orderNumber = engineMetadataService.getAttr(engine, "order");

        // Since the DB isn't regularly cleared, and engine files may disappear
        // without us knowing, we may already have an engine in this slot. If
        // that happens, we just skip it - it will be added later on as an
        // unsorted engine. This problem will sort itself out when we call
        // _saveSortedEngineList at shutdown.
        if (orderNumber && !this.__sortedEngines[orderNumber-1]) {
          this.__sortedEngines[orderNumber-1] = engine;
          addedEngines[engine.name] = engine;
        } else {
          // We need to call _saveSortedEngines so this gets sorted out.
          this._needToSetOrderPrefs = true;
        }
      }

      // Filter out any nulls for engines that may have been removed
      var filteredEngines = this.__sortedEngines.filter(function(a) { return !!a; });
      if (this.__sortedEngines.length != filteredEngines.length)
        this._needToSetOrderPrefs = true;
      this.__sortedEngines = filteredEngines;

    } else {
      // The DB isn't being used, so just read the engine order from the prefs
      var i = 0;
      var engineName;
      var prefName;

      try {
        var extras =
          Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

        for each (prefName in extras) {
          engineName = Services.prefs.getCharPref(prefName);

          engine = this._engines[engineName];
          if (!engine || engine.name in addedEngines)
            continue;

          this.__sortedEngines.push(engine);
          addedEngines[engine.name] = engine;
        }
      }
      catch (e) { }

      while (true) {
        engineName = getLocalizedPref(BROWSER_SEARCH_PREF + "order." + (++i));
        if (!engineName)
          break;

        engine = this._engines[engineName];
        if (!engine || engine.name in addedEngines)
          continue;
        
        this.__sortedEngines.push(engine);
        addedEngines[engine.name] = engine;
      }
    }

    // Array for the remaining engines, alphabetically sorted
    var alphaEngines = [];

    for each (engine in this._engines) {
      if (!(engine.name in addedEngines))
        alphaEngines.push(this._engines[engine.name]);
    }
    alphaEngines = alphaEngines.sort(function (a, b) {
                                       return a.name.localeCompare(b.name);
                                     });
    return this.__sortedEngines = this.__sortedEngines.concat(alphaEngines);
  },

  /**
   * Converts a Sherlock file and its icon into the custom XML format used by
   * the Search Service. Saves the engine's icon (if present) into the XML as a
   * data: URI and changes the extension of the source file from ".src" to
   * ".xml". The engine data is then written to the file as XML.
   * @param aEngine
   *        The Engine object that needs to be converted.
   * @param aBaseName
   *        The basename of the Sherlock file.
   *          Example: "foo" for file "foo.src".
   *
   * @throws NS_ERROR_FAILURE if the file could not be converted.
   *
   * @see nsIURL::fileBaseName
   */
  _convertSherlockFile: function SRCH_SVC_convertSherlock(aEngine, aBaseName) {
    ENSURE_WARN(aEngine._file, "can't convert an engine with no file",
                Cr.NS_ERROR_UNEXPECTED);

    var oldSherlockFile = aEngine._file;

    // Back up the old file
    try {
      var backupDir = oldSherlockFile.parent;
      backupDir.append("searchplugins-backup");

      if (!backupDir.exists())
        backupDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);

      oldSherlockFile.copyTo(backupDir, null);
    } catch (ex) {
      // Just bail. Engines that can't be backed up won't be converted, but
      // engines that aren't converted are loaded as readonly.
      FAIL("_convertSherlockFile: Couldn't back up " + oldSherlockFile.path +
           ":\n" + ex, Cr.NS_ERROR_FAILURE);
    }

    // Rename the file, but don't clobber existing files
    var newXMLFile = oldSherlockFile.parent.clone();
    newXMLFile.append(aBaseName + "." + XML_FILE_EXT);

    if (newXMLFile.exists()) {
      // There is an existing file with this name, create a unique file
      newXMLFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
    }

    // Rename the .src file to .xml
    oldSherlockFile.moveTo(null, newXMLFile.leafName);

    aEngine._file = newXMLFile;

    // Write the converted engine to disk
    aEngine._serializeToFile();

    // Update the engine's _type.
    aEngine._type = SEARCH_TYPE_MOZSEARCH;

    // See if it has a corresponding icon
    try {
      var icon = this._findSherlockIcon(aEngine._file, aBaseName);
      if (icon && icon.fileSize < MAX_ICON_SIZE) {
        // Use this as the engine's icon
        var bStream = Cc["@mozilla.org/binaryinputstream;1"].
                        createInstance(Ci.nsIBinaryInputStream);
        var fileInStream = Cc["@mozilla.org/network/file-input-stream;1"].
                           createInstance(Ci.nsIFileInputStream);

        fileInStream.init(icon, MODE_RDONLY, PERMS_FILE, 0);
        bStream.setInputStream(fileInStream);

        var bytes = [];
        while (bStream.available() != 0)
          bytes = bytes.concat(bStream.readByteArray(bStream.available()));
        bStream.close();

        // Convert the byte array to a base64-encoded string
        var str = btoa(String.fromCharCode.apply(null, bytes));

        aEngine._iconURI = makeURI(ICON_DATAURL_PREFIX + str);
        LOG("_importSherlockEngine: Set sherlock iconURI to: \"" +
            aEngine._iconURL + "\"");

        // Write the engine to disk to save changes
        aEngine._serializeToFile();

        // Delete the icon now that we're sure everything's been saved
        icon.remove(false);
      }
    } catch (ex) { LOG("_convertSherlockFile: Error setting icon:\n" + ex); }
  },

  /**
   * Finds an icon associated to a given Sherlock file. Searches the provided
   * file's parent directory looking for files with the same base name and one
   * of the file extensions in SHERLOCK_ICON_EXTENSIONS.
   * @param aEngineFile
   *        The Sherlock plugin file.
   * @param aBaseName
   *        The basename of the Sherlock file.
   *          Example: "foo" for file "foo.src".
   * @see nsIURL::fileBaseName
   */
  _findSherlockIcon: function SRCH_SVC_findSherlock(aEngineFile, aBaseName) {
    for (var i = 0; i < SHERLOCK_ICON_EXTENSIONS.length; i++) {
      var icon = aEngineFile.parent.clone();
      icon.append(aBaseName + SHERLOCK_ICON_EXTENSIONS[i]);
      if (icon.exists() && icon.isFile())
        return icon;
    }
    return null;
  },

  /**
   * Get a sorted array of engines.
   * @param aWithHidden
   *        True if hidden plugins should be included in the result.
   */
  _getSortedEngines: function SRCH_SVC_getSorted(aWithHidden) {
    if (aWithHidden)
      return this._sortedEngines;

    return this._sortedEngines.filter(function (engine) {
                                        return !engine.hidden;
                                      });
  },

  // nsIBrowserSearchService
  getEngines: function SRCH_SVC_getEngines(aCount) {
    LOG("getEngines: getting all engines");
    var engines = this._getSortedEngines(true);
    aCount.value = engines.length;
    return engines;
  },

  getVisibleEngines: function SRCH_SVC_getVisible(aCount) {
    LOG("getVisibleEngines: getting all visible engines");
    var engines = this._getSortedEngines(false);
    aCount.value = engines.length;
    return engines;
  },

  getDefaultEngines: function SRCH_SVC_getDefault(aCount) {
    function isDefault(engine) {
      return engine._isDefault;
    };
    var engines = this._sortedEngines.filter(isDefault);
    var engineOrder = {};
    var engineName;
    var i = 1;

    // Build a list of engines which we have ordering information for.
    // We're rebuilding the list here because _sortedEngines contain the
    // current order, but we want the original order.

    // First, look at the "browser.search.order.extra" branch.
    try {
      var extras = Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

      for each (var prefName in extras) {
        engineName = Services.prefs.getCharPref(prefName);

        if (!(engineName in engineOrder))
          engineOrder[engineName] = i++;
      }
    } catch (e) {
      LOG("Getting extra order prefs failed: " + e);
    }

    // Now look through the "browser.search.order" branch.
    for (var j = 1; ; j++) {
      engineName = getLocalizedPref(BROWSER_SEARCH_PREF + "order." + j);
      if (!engineName)
        break;

      if (!(engineName in engineOrder))
        engineOrder[engineName] = i++;
    }

    LOG("getDefaultEngines: engineOrder: " + engineOrder.toSource());

    function compareEngines (a, b) {
      var aIdx = engineOrder[a.name];
      var bIdx = engineOrder[b.name];

      if (aIdx && bIdx)
        return aIdx - bIdx;
      if (aIdx)
        return -1;
      if (bIdx)
        return 1;

      return a.name.localeCompare(b.name);
    }
    engines.sort(compareEngines);

    aCount.value = engines.length;
    return engines;
  },

  getEngineByName: function SRCH_SVC_getEngineByName(aEngineName) {
    return this._engines[aEngineName] || null;
  },

  getEngineByAlias: function SRCH_SVC_getEngineByAlias(aAlias) {
    for (var engineName in this._engines) {
      var engine = this._engines[engineName];
      if (engine && engine.alias == aAlias)
        return engine;
    }
    return null;
  },

  addEngineWithDetails: function SRCH_SVC_addEWD(aName, aIconURL, aAlias,
                                                 aDescription, aMethod,
                                                 aTemplate) {
    if (!aName)
      FAIL("Invalid name passed to addEngineWithDetails!");
    if (!aMethod)
      FAIL("Invalid method passed to addEngineWithDetails!");
    if (!aTemplate)
      FAIL("Invalid template passed to addEngineWithDetails!");
    if (this._engines[aName])
      FAIL("An engine with that name already exists!", Cr.NS_ERROR_FILE_ALREADY_EXISTS);

    var engine = new Engine(getSanitizedFile(aName), SEARCH_DATA_XML, false);
    engine._initFromMetadata(aName, aIconURL, aAlias, aDescription,
                             aMethod, aTemplate);
    this._addEngineToStore(engine);
    this._batchCacheInvalidation();
  },

  addEngine: function SRCH_SVC_addEngine(aEngineURL, aDataType, aIconURL,
                                         aConfirm) {
    LOG("addEngine: Adding \"" + aEngineURL + "\".");
    try {
      var uri = makeURI(aEngineURL);
      var engine = new Engine(uri, aDataType, false);
      engine._initFromURI();
    } catch (ex) {
      FAIL("addEngine: Error adding engine:\n" + ex, Cr.NS_ERROR_FAILURE);
    }
    engine._setIcon(aIconURL, false);
    engine._confirm = aConfirm;
  },

  removeEngine: function SRCH_SVC_removeEngine(aEngine) {
    if (!aEngine)
      FAIL("no engine passed to removeEngine!");

    var engineToRemove = null;
    for (var e in this._engines)
      if (aEngine.wrappedJSObject == this._engines[e])
        engineToRemove = this._engines[e];

    if (!engineToRemove)
      FAIL("removeEngine: Can't find engine to remove!", Cr.NS_ERROR_FILE_NOT_FOUND);

    if (engineToRemove == this.currentEngine)
      this._currentEngine = null;

    if (engineToRemove._readOnly) {
      // Just hide it (the "hidden" setter will notify) and remove its alias to
      // avoid future conflicts with other engines.
      engineToRemove.hidden = true;
      engineToRemove.alias = null;
    } else {
      // Cancel the lazy serialization timer if it's running
      if (engineToRemove._serializeTimer) {
        engineToRemove._serializeTimer.cancel();
        engineToRemove._serializeTimer = null;
      }

      // Remove the engine file from disk (this might throw)
      engineToRemove._remove();
      engineToRemove._file = null;

      // Remove the engine from _sortedEngines
      var index = this._sortedEngines.indexOf(engineToRemove);
      if (index == -1)
        FAIL("Can't find engine to remove in _sortedEngines!", Cr.NS_ERROR_FAILURE);
      this.__sortedEngines.splice(index, 1);

      // Remove the engine from the internal store
      delete this._engines[engineToRemove.name];

      notifyAction(engineToRemove, SEARCH_ENGINE_REMOVED);

      // Since we removed an engine, we need to update the preferences.
      this._needToSetOrderPrefs = true;
    }
  },

  moveEngine: function SRCH_SVC_moveEngine(aEngine, aNewIndex) {
    if ((aNewIndex > this._sortedEngines.length) || (aNewIndex < 0))
      FAIL("SRCH_SVC_moveEngine: Index out of bounds!");
    if (!(aEngine instanceof Ci.nsISearchEngine))
      FAIL("SRCH_SVC_moveEngine: Invalid engine passed to moveEngine!");
    if (aEngine.hidden)
      FAIL("moveEngine: Can't move a hidden engine!", Cr.NS_ERROR_FAILURE);

    var engine = aEngine.wrappedJSObject;

    var currentIndex = this._sortedEngines.indexOf(engine);
    if (currentIndex == -1)
      FAIL("moveEngine: Can't find engine to move!", Cr.NS_ERROR_UNEXPECTED);

    // Our callers only take into account non-hidden engines when calculating
    // aNewIndex, but we need to move it in the array of all engines, so we
    // need to adjust aNewIndex accordingly. To do this, we count the number
    // of hidden engines in the list before the engine that we're taking the
    // place of. We do this by first finding newIndexEngine (the engine that
    // we were supposed to replace) and then iterating through the complete 
    // engine list until we reach it, increasing aNewIndex for each hidden
    // engine we find on our way there.
    //
    // This could be further simplified by having our caller pass in
    // newIndexEngine directly instead of aNewIndex.
    var newIndexEngine = this._getSortedEngines(false)[aNewIndex];
    if (!newIndexEngine)
      FAIL("moveEngine: Can't find engine to replace!", Cr.NS_ERROR_UNEXPECTED);

    for (var i = 0; i < this._sortedEngines.length; ++i) {
      if (newIndexEngine == this._sortedEngines[i])
        break;
      if (this._sortedEngines[i].hidden)
        aNewIndex++;
    }

    if (currentIndex == aNewIndex)
      return; // nothing to do!

    // Move the engine
    var movedEngine = this.__sortedEngines.splice(currentIndex, 1)[0];
    this.__sortedEngines.splice(aNewIndex, 0, movedEngine);

    notifyAction(engine, SEARCH_ENGINE_CHANGED);

    // Since we moved an engine, we need to update the preferences.
    this._needToSetOrderPrefs = true;
  },

  restoreDefaultEngines: function SRCH_SVC_resetDefaultEngines() {
    for each (var e in this._engines) {
      // Unhide all default engines
      if (e.hidden && e._isDefault)
        e.hidden = false;
    }
  },

  get originalDefaultEngine() {
    const defPref = BROWSER_SEARCH_PREF + "defaultenginename";
    return this.getEngineByName(getLocalizedPref(defPref, ""));
  },

  get defaultEngine() {
    let defaultEngine = this.originalDefaultEngine;
    if (!defaultEngine || defaultEngine.hidden)
      defaultEngine = this._getSortedEngines(false)[0] || null;
    return defaultEngine;
  },

  get currentEngine() {
    if (!this._currentEngine) {
      let selectedEngine = getLocalizedPref(BROWSER_SEARCH_PREF +
                                            "selectedEngine");
      this._currentEngine = this.getEngineByName(selectedEngine);
    }

    if (!this._currentEngine || this._currentEngine.hidden)
      this._currentEngine = this.defaultEngine;
    return this._currentEngine;
  },
  set currentEngine(val) {
    if (!(val instanceof Ci.nsISearchEngine))
      FAIL("Invalid argument passed to currentEngine setter");

    var newCurrentEngine = this.getEngineByName(val.name);
    if (!newCurrentEngine)
      FAIL("Can't find engine in store!", Cr.NS_ERROR_UNEXPECTED);

    this._currentEngine = newCurrentEngine;

    var currentEnginePref = BROWSER_SEARCH_PREF + "selectedEngine";

    if (this._currentEngine == this.defaultEngine) {
      Services.prefs.clearUserPref(currentEnginePref);
    }
    else {
      setLocalizedPref(currentEnginePref, this._currentEngine.name);
    }

    notifyAction(this._currentEngine, SEARCH_ENGINE_CURRENT);
  },

  // nsIObserver
  observe: function SRCH_SVC_observe(aEngine, aTopic, aVerb) {
    switch (aTopic) {
      case SEARCH_ENGINE_TOPIC:
        switch (aVerb) {
          case SEARCH_ENGINE_LOADED:
            var engine = aEngine.QueryInterface(Ci.nsISearchEngine);
            LOG("nsSearchService::observe: Done installation of " + engine.name
                + ".");
            this._addEngineToStore(engine.wrappedJSObject);
            if (engine.wrappedJSObject._useNow) {
              LOG("nsSearchService::observe: setting current");
              this.currentEngine = aEngine;
            }
            this._batchCacheInvalidation();
            break;
          case SEARCH_ENGINE_CHANGED:
          case SEARCH_ENGINE_REMOVED:
            this._batchCacheInvalidation();
            break;
        }
        break;

      case QUIT_APPLICATION_TOPIC:
        this._removeObservers();
        this._saveSortedEngineList();
        if (this._batchTimer) {
          // Flush to disk immediately
          this._batchTimer.cancel();
          this._buildCache();
        }
        engineMetadataService.flush();
        break;
    }
  },

  // nsITimerCallback
  notify: function SRCH_SVC_notify(aTimer) {
    LOG("_notify: checking for updates");

    if (!getBoolPref(BROWSER_SEARCH_PREF + "update", true))
      return;

    // Our timer has expired, but unfortunately, we can't get any data from it.
    // Therefore, we need to walk our engine-list, looking for expired engines
    var currentTime = Date.now();
    LOG("currentTime: " + currentTime);
    for each (engine in this._engines) {
      engine = engine.wrappedJSObject;
      if (!engine._hasUpdates)
        continue;

      LOG("checking " + engine.name);

      var expirTime = engineMetadataService.getAttr(engine, "updateexpir");
      LOG("expirTime: " + expirTime + "\nupdateURL: " + engine._updateURL +
          "\niconUpdateURL: " + engine._iconUpdateURL);

      var engineExpired = expirTime <= currentTime;

      if (!expirTime || !engineExpired) {
        LOG("skipping engine");
        continue;
      }

      LOG(engine.name + " has expired");

      engineUpdateService.update(engine);

      // Schedule the next update
      engineUpdateService.scheduleNextUpdate(engine);

    } // end engine iteration
  },

  _addObservers: function SRCH_SVC_addObservers() {
    Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC, false);
    Services.obs.addObserver(this, QUIT_APPLICATION_TOPIC, false);
  },

  _removeObservers: function SRCH_SVC_removeObservers() {
    Services.obs.removeObserver(this, SEARCH_ENGINE_TOPIC);
    Services.obs.removeObserver(this, QUIT_APPLICATION_TOPIC);
  },

  QueryInterface: function SRCH_SVC_QI(aIID) {
    if (aIID.equals(Ci.nsIBrowserSearchService) ||
        aIID.equals(Ci.nsIObserver)             ||
        aIID.equals(Ci.nsITimerCallback)        ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var engineMetadataService = {
  /**
   * @type {nsIFile|null} The file holding the metadata.
   */
  get _jsonFile() {
    delete this._jsonFile;
    return this._jsonFile = FileUtils.getFile(NS_APP_USER_PROFILE_50_DIR,
                                              ["search-metadata.json"]);
  },

  /**
   * Lazy getter for the file containing json data.
   */
  get _store() {
    delete this._store;
    return this._store = this._loadStore();
  },

  // Perform loading the first time |_store| is accessed.
  _loadStore: function() {
    let jsonFile = this._jsonFile;
    if (!jsonFile.exists()) {
      LOG("loadStore: search-metadata.json does not exist");

      // First check to see whether there's an existing SQLite DB to migrate
      let store = this._migrateOldDB();
      if (store) {
        // Commit the migrated store to disk immediately
        LOG("Committing the migrated store to disk");
        this._commit(store);
        return store;
      }

       // Migration failed, or this is a first-run - just use an empty store
      return {};
    }

    LOG("loadStore: attempting to load store from JSON file");
    try {
      return parseJsonFromStream(NetUtil.newChannel(jsonFile).open());
    } catch (x) {
      LOG("loadStore failed to load file: "+x);
      return {};
    }
  },

  getAttr: function epsGetAttr(engine, name) {
    let record = this._store[engine._id];
    if (!record) {
      return null;
    }

    // attr names must be lower case
    return record[name.toLowerCase()];
  },

  _setAttr: function epsSetAttr(engine, name, value) {
    // attr names must be lower case
    name = name.toLowerCase();
    let db = this._store;
    let record = db[engine._id];
    if (!record) {
      record = db[engine._id] = {};
    }
    if (record[name] != value) {
      record[name] = value;
      return true;
    }
    return false;
  },

  /**
   * Set one metadata attribute for an engine.
   *
   * If an actual change has taken place, the attribute is committed
   * automatically (and lazily), using this._commit.
   *
   * @param {nsISearchEngine} engine The engine to update.
   * @param {string} key The name of the attribute. Case-insensitive. In
   * the current implementation, this _must not_ conflict with properties
   * of |Object|.
   * @param {*} value A value to store.
   */
  setAttr: function epsSetAttr(engine, key, value) {
    if (this._setAttr(engine, key, value)) {
      this._commit();
    }
  },

  /**
   * Bulk set metadata attributes for a number of engines.
   *
   * If actual changes have taken place, the store is committed
   * automatically (and lazily), using this._commit.
   *
   * @param {Array.<{engine: nsISearchEngine, key: string, value: *}>} changes
   * The list of changes to effect. See |setAttr| for the documentation of
   * |engine|, |key|, |value|.
   */
  setAttrs: function epsSetAttrs(changes) {
    let self = this;
    let changed = false;
    changes.forEach(function(change) {
      changed |= self._setAttr(change.engine, change.key, change.value);
    });
    if (changed) {
      this._commit();
    }
  },

  /**
   * Flush any waiting write.
   */
  flush: function epsFlush() {
    if (this._lazyWriter) {
      this._lazyWriter.flush();
    }
  },

  /**
   * Migrate search.sqlite
   *
   * Notes:
   * - we do not remove search.sqlite after migration, so as to allow
   * downgrading and forensics;
   */
  _migrateOldDB: function SRCH_SVC_EMS_migrate() {
    LOG("SRCH_SVC_EMS_migrate start");
    let sqliteFile = FileUtils.getFile(NS_APP_USER_PROFILE_50_DIR,
                                       ["search.sqlite"]);
    if (!sqliteFile.exists()) {
      LOG("SRCH_SVC_EMS_migrate search.sqlite does not exist");
      return null;
    }
    let store = {};
    try {
      LOG("SRCH_SVC_EMS_migrate Migrating data from SQL");
      const sqliteDb = Services.storage.openDatabase(sqliteFile);
      const statement = sqliteDb.createStatement("SELECT * from engine_data");
      while (statement.executeStep()) {
        let row = statement.row;
        let engine = row.engineid;
        let name   = row.name;
        let value  = row.value;
        if (!store[engine]) {
          store[engine] = {};
        }
        store[engine][name] = value;
      }
      statement.finalize();
      sqliteDb.close();
    } catch (ex) {
      LOG("SRCH_SVC_EMS_migrate failed: " + ex);
      return null;
    }
    return store;
  },

  /**
   * Commit changes to disk, asynchronously.
   *
   * Calls to this function are actually delayed by LAZY_SERIALIZE_DELAY
   * (= 100ms). If the function is called again before the expiration of
   * the delay, commits are merged and the function is again delayed by
   * the same amount of time.
   *
   * @param aStore is an optional parameter specifying the object to serialize.
   *               If not specified, this._store is used.
   */
  _commit: function epsCommit(aStore) {
    LOG("epsCommit: start");

    let store = aStore || this._store;
    if (!store) {
      LOG("epsCommit: nothing to do");
      return;
    }

    if (!this._lazyWriter) {
      LOG("epsCommit: initializing lazy writer");
      let jsonFile = this._jsonFile;
      function writeCommit() {
        LOG("epsWriteCommit: start");
        let ostream = FileUtils.
          openSafeFileOutputStream(jsonFile,
                                   MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE);

        // Obtain a converter to convert our data to a UTF-8 encoded input stream.
        let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
          createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = "UTF-8";

        let callback = function(result) {
          if (Components.isSuccessCode(result)) {
            Services.obs.notifyObservers(null,
                                         SEARCH_SERVICE_TOPIC,
                                         SEARCH_SERVICE_METADATA_WRITTEN);
          }
          LOG("epsWriteCommit: done " + result);
        };
        // Asynchronously copy the data to the file.
        let istream = converter.convertToInputStream(JSON.stringify(store));
        NetUtil.asyncCopy(istream, ostream, callback);
      }
      this._lazyWriter = new Lazy(writeCommit);
    }
    LOG("epsCommit: (re)setting timer");
    this._lazyWriter.go();
  },
  _lazyWriter: null,
};

const SEARCH_UPDATE_LOG_PREFIX = "*** Search update: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout, if the search
 * logging pref (browser.search.update.log) is set to true.
 */
function ULOG(aText) {
  if (getBoolPref(BROWSER_SEARCH_PREF + "update.log", false)) {
    dump(SEARCH_UPDATE_LOG_PREFIX + aText + "\n");
    Services.console.logStringMessage(aText);
  }
}

var engineUpdateService = {
  scheduleNextUpdate: function eus_scheduleNextUpdate(aEngine) {
    var interval = aEngine._updateInterval || SEARCH_DEFAULT_UPDATE_INTERVAL;
    var milliseconds = interval * 86400000; // |interval| is in days
    engineMetadataService.setAttr(aEngine, "updateexpir",
                                  Date.now() + milliseconds);
  },

  update: function eus_Update(aEngine) {
    let engine = aEngine.wrappedJSObject;
    ULOG("update called for " + aEngine._name);
    if (!getBoolPref(BROWSER_SEARCH_PREF + "update", true) || !engine._hasUpdates)
      return;

    // We use the cache to store updated app engines, so refuse to update if the
    // cache is disabled.
    if (engine._readOnly &&
        !getBoolPref(BROWSER_SEARCH_PREF + "cache.enabled", true))
      return;

    let testEngine = null;
    let updateURL = engine._getURLOfType(URLTYPE_OPENSEARCH);
    let updateURI = (updateURL && updateURL._hasRelation("self")) ? 
                     updateURL.getSubmission("", engine).uri :
                     makeURI(engine._updateURL);
    if (updateURI) {
      if (engine._isDefault && !updateURI.schemeIs("https")) {
        ULOG("Invalid scheme for default engine update");
        return;
      }

      let dataType = engineMetadataService.getAttr(engine, "updatedatatype");
      if (!dataType) {
        ULOG("No loadtype to update engine!");
        return;
      }

      ULOG("updating " + engine.name + " from " + updateURI.spec);
      testEngine = new Engine(updateURI, dataType, false);
      testEngine._engineToUpdate = engine;
      testEngine._initFromURI();
    } else
      ULOG("invalid updateURI");

    if (engine._iconUpdateURL) {
      // If we're updating the engine too, use the new engine object,
      // otherwise use the existing engine object.
      (testEngine || engine)._setIcon(engine._iconUpdateURL, true);
    }
  }
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([SearchService]);

#include ../../../toolkit/content/debug.js
