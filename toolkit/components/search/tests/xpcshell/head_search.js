/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

var { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
var { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
var { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const PREF_SEARCH_URL = "geoSpecificDefaults.url";
const NS_APP_SEARCH_DIR = "SrchPlugns";

const MODE_RDONLY = FileUtils.MODE_RDONLY;
const MODE_WRONLY = FileUtils.MODE_WRONLY;
const MODE_CREATE = FileUtils.MODE_CREATE;
const MODE_TRUNCATE = FileUtils.MODE_TRUNCATE;

const CACHE_FILENAME = "search.json.mozlz4";

// nsSearchService.js uses Services.appinfo.name to build a salt for a hash.
// eslint-disable-next-line mozilla/use-services
var XULRuntime = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime);

// Expand the amount of information available in error logs
Services.prefs.setBoolPref("browser.search.log", true);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gModernConfig",
  SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
  false
);

AddonTestUtils.init(this, false);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

// Allow telemetry probes which may otherwise be disabled for some applications (e.g. Thunderbird)
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

/**
 * Load engines from test data located in particular folders.
 *
 * @param {string} [folder]
 *   The folder name to use.
 * @param {string} [subFolder]
 *   The subfolder to use, if any.
 * @param {array} [config]
 *   An array which contains the configuration to set.
 * @returns {object|null}
 *   If this is the modern configuration, returns a stub for the method
 *   that the configuration is obtained from.
 */
async function useTestEngines(
  folder = "data",
  subFolder = null,
  config = null
) {
  let url = `resource://test/${folder}/`;
  if (subFolder) {
    url += `${subFolder}/`;
  }
  let resProt = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-extensions", Services.io.newURI(url));
  if (gModernConfig) {
    const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
    if (config) {
      return sinon.stub(settings, "get").returns(config);
    }
    let chan = NetUtil.newChannel({
      uri: "resource://search-extensions/engines.json",
      loadUsingSystemPrincipal: true,
    });
    let json = parseJsonFromStream(chan.open());
    return sinon.stub(settings, "get").returns(json.data);
  }
  return null;
}

async function promiseCacheData() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
  let bytes = await OS.File.read(path, { compression: "lz4" });
  return JSON.parse(new TextDecoder().decode(bytes));
}

function promiseSaveCacheData(data) {
  return OS.File.writeAtomic(
    OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME),
    new TextEncoder().encode(JSON.stringify(data)),
    { compression: "lz4" }
  );
}

async function promiseEngineMetadata() {
  let cache = await promiseCacheData();
  let data = {};
  for (let engine of cache.engines) {
    data[engine._shortName] = engine._metaData;
  }
  return data;
}

async function promiseGlobalMetadata() {
  return (await promiseCacheData()).metaData;
}

async function promiseSaveGlobalMetadata(globalData) {
  let data = await promiseCacheData();
  data.metaData = globalData;
  await promiseSaveCacheData(data);
}

async function forceExpiration() {
  let metadata = await promiseGlobalMetadata();

  // Make the current geodefaults expire 1s ago.
  metadata.searchDefaultExpir = Date.now() - 1000;
  await promiseSaveGlobalMetadata(metadata);
}

function promiseDefaultNotification(type = "normal") {
  return SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE[
      type == "private" ? "DEFAULT_PRIVATE" : "DEFAULT"
    ],
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
}

/**
 * Clean the profile of any cache file left from a previous run.
 *
 * @returns {boolean}
 *   Indicates if the cache file existed.
 */
function removeCacheFile() {
  let file = do_get_profile().clone();
  file.append(CACHE_FILENAME);
  if (file.exists()) {
    file.remove(false);
    return true;
  }
  return false;
}

/**
 * isUSTimezone taken from nsSearchService.js
 *
 * @returns {boolean}
 */
function isUSTimezone() {
  // Timezone assumptions! We assume that if the system clock's timezone is
  // between Newfoundland and Hawaii, that the user is in North America.

  // This includes all of South America as well, but we have relatively few
  // en-US users there, so that's OK.

  // 150 minutes = 2.5 hours (UTC-2.5), which is
  // Newfoundland Daylight Time (http://www.timeanddate.com/time/zones/ndt)

  // 600 minutes = 10 hours (UTC-10), which is
  // Hawaii-Aleutian Standard Time (http://www.timeanddate.com/time/zones/hast)

  let UTCOffset = new Date().getTimezoneOffset();
  return UTCOffset >= 150 && UTCOffset <= 600;
}

const kDefaultenginenamePref = "browser.search.defaultenginename";
const kTestEngineName = "Test search engine";
const TOPIC_LOCALES_CHANGE = "intl:app-locales-changed";

/**
 * Loads the current default engine list.json via parsing the json manually.
 *
 * @param {boolean} isUS
 *   If this is false, the requested locale will be checked, otherwise the
 *   US region will be used if it exists.
 * @param {boolean} privateMode
 *   If this is true, then the engine for private mode is returned.
 * @returns {string}
 *   Returns the name of the private engine.
 */
function getDefaultEngineName(isUS = false, privateMode = false) {
  // The list of visibleDefaultEngines needs to match or the cache will be ignored.
  let chan = NetUtil.newChannel({
    uri: "resource://search-extensions/list.json",
    loadUsingSystemPrincipal: true,
  });
  const settingName = privateMode ? "searchPrivateDefault" : "searchDefault";
  let searchSettings = parseJsonFromStream(chan.open());
  let defaultEngineName = searchSettings.default[settingName];

  if (!isUS) {
    isUS = Services.locale.requestedLocale == "en-US" && isUSTimezone();
  }

  if (isUS && "US" in searchSettings && settingName in searchSettings.US) {
    defaultEngineName = searchSettings.US[settingName];
  }
  return defaultEngineName;
}

function getDefaultEngineList(isUS) {
  // The list of visibleDefaultEngines needs to match or the cache will be ignored.
  let chan = NetUtil.newChannel({
    uri: "resource://search-extensions/list.json",
    loadUsingSystemPrincipal: true,
  });
  let json = parseJsonFromStream(chan.open());
  let visibleDefaultEngines = json.default.visibleDefaultEngines;

  if (isUS === undefined) {
    isUS = Services.locale.requestedLocale == "en-US" && isUSTimezone();
  }

  if (isUS) {
    let searchSettings = json.locales["en-US"];
    if (
      "US" in searchSettings &&
      "visibleDefaultEngines" in searchSettings.US
    ) {
      visibleDefaultEngines = searchSettings.US.visibleDefaultEngines;
    }
    // From nsSearchService.js
    let searchRegion = "US";
    if ("regionOverrides" in json && searchRegion in json.regionOverrides) {
      for (let engine in json.regionOverrides[searchRegion]) {
        let index = visibleDefaultEngines.indexOf(engine);
        if (index > -1) {
          visibleDefaultEngines[index] =
            json.regionOverrides[searchRegion][engine];
        }
      }
    }
  }

  return visibleDefaultEngines;
}

/**
 * Waits for the cache file to be saved.
 * @returns {Promise} Resolved when the cache file is saved.
 */
function promiseAfterCache() {
  return SearchTestUtils.promiseSearchNotification(
    "write-cache-to-disk-complete"
  );
}

function parseJsonFromStream(aInputStream) {
  let bytes = NetUtil.readInputStream(aInputStream, aInputStream.available());
  return JSON.parse(new TextDecoder().decode(bytes));
}

/**
 * Read a JSON file and return the JS object
 *
 * @param {nsIFile} aFile
 *   The file to read.
 * @returns {object|false}
 *   Returns the JSON object if the file was successfully read,
 *   false otherwise.
 */
function readJSONFile(aFile) {
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  try {
    stream.init(aFile, MODE_RDONLY, FileUtils.PERMS_FILE, 0);
    return parseJsonFromStream(stream, stream.available());
  } catch (ex) {
    dump("search test: readJSONFile: Error reading JSON file: " + ex + "\n");
  } finally {
    stream.close();
  }
  return false;
}

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 *
 * @param {object} expectedObj
 * @param {object} actualObj
 * @param {function} skipProp
 *   A function that is called with the property name and its value, to see if
 *   testing that property should be skipped or not.
 */
function isSubObjectOf(expectedObj, actualObj, skipProp) {
  for (let prop in expectedObj) {
    if (skipProp && skipProp(prop, expectedObj[prop])) {
      continue;
    }
    if (expectedObj[prop] instanceof Object) {
      Assert.equal(
        actualObj[prop].length,
        expectedObj[prop].length,
        `Should have the correct length for property ${prop}`
      );
      isSubObjectOf(expectedObj[prop], actualObj[prop], skipProp);
    } else {
      Assert.equal(
        actualObj[prop],
        expectedObj[prop],
        `Should have the correct value for property ${prop}`
      );
    }
  }
}

/**
 * After useHttpServer() is called, this string contains the URL of the "data"
 * directory, including the final slash.
 */
var gDataUrl;

/**
 * Initializes the HTTP server and ensures that it is terminated when tests end.
 *
 * @returns {HttpServer}
 *   The HttpServer object in case further customization is needed.
 */
function useHttpServer() {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  gDataUrl = "http://localhost:" + httpServer.identity.primaryPort + "/data/";
  registerCleanupFunction(async function cleanup_httpServer() {
    await new Promise(resolve => {
      httpServer.stop(resolve);
    });
  });
  return httpServer;
}

async function withGeoServer(
  testFn,
  {
    visibleDefaultEngines = null,
    searchDefault = null,
    geoLookupData = null,
    preGeolookupPromise = Promise.resolve,
    cohort = null,
    intval200 = 86400 * 365,
    intval503 = 86400,
    delay = 0,
    path = "lookup_defaults",
  } = {}
) {
  let srv = new HttpServer();
  let gRequests = [];
  srv.registerPathHandler("/lookup_defaults", (metadata, response) => {
    let data = {
      interval: intval200,
      settings: { searchDefault: searchDefault ?? kTestEngineName },
    };
    if (cohort) {
      data.cohort = cohort;
    }
    if (visibleDefaultEngines) {
      data.settings.visibleDefaultEngines = visibleDefaultEngines;
    }
    response.processAsync();
    setTimeout(() => {
      response.setStatusLine("1.1", 200, "OK");
      response.write(JSON.stringify(data));
      response.finish();
      gRequests.push(metadata);
    }, delay);
  });

  srv.registerPathHandler("/lookup_fail", (metadata, response) => {
    response.processAsync();
    setTimeout(() => {
      response.setStatusLine("1.1", 404, "Not Found");
      response.finish();
      gRequests.push(metadata);
    }, delay);
  });

  srv.registerPathHandler("/lookup_unavailable", (metadata, response) => {
    response.processAsync();
    setTimeout(() => {
      response.setStatusLine("1.1", 503, "Service Unavailable");
      response.setHeader("Retry-After", intval503.toString());
      response.finish();
      gRequests.push(metadata);
    }, delay);
  });

  srv.registerPathHandler("/lookup_geoip", async (metadata, response) => {
    response.processAsync();
    await preGeolookupPromise;
    response.setStatusLine("1.1", 200, "OK");
    response.write(JSON.stringify(geoLookupData));
    response.finish();
    gRequests.push(metadata);
  });

  srv.start(-1);

  let url = `http://localhost:${srv.identity.primaryPort}/${path}?`;
  let defaultBranch = Services.prefs.getDefaultBranch(
    SearchUtils.BROWSER_SEARCH_PREF
  );
  let originalURL = defaultBranch.getCharPref(PREF_SEARCH_URL, "");
  defaultBranch.setCharPref(PREF_SEARCH_URL, url);
  // Set a bogus user value so that running the test ensures we ignore it.
  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + PREF_SEARCH_URL,
    "about:blank"
  );

  let geoLookupUrl = geoLookupData
    ? `http://localhost:${srv.identity.primaryPort}/lookup_geoip`
    : 'data:application/json,{"country_code": "FR"}';
  Services.prefs.setCharPref("geo.provider-country.network.url", geoLookupUrl);

  try {
    await testFn(gRequests);
  } finally {
    srv.stop(() => {});
    defaultBranch.setCharPref(PREF_SEARCH_URL, originalURL);
    Services.prefs.clearUserPref(
      SearchUtils.BROWSER_SEARCH_PREF + PREF_SEARCH_URL
    );
    Services.prefs.clearUserPref("geo.provider-country.network.url");
  }
}

function checkNoRequest(requests) {
  Assert.equal(requests.length, 0);
}

function checkRequest(requests, cohort = "") {
  Assert.equal(requests.length, 1);
  let req = requests.pop();
  Assert.equal(req._method, "GET");
  Assert.equal(req._queryString, cohort ? "/" + cohort : "");
}

/**
 * Adds test engines and returns a promise resolved when they are installed.
 *
 * The engines are added in the given order.
 *
 * @param {Array<object>} aItems
 *   Array of objects with the following properties:
 *   {
 *     name: Engine name, used to wait for it to be loaded.
 *     xmlFileName: Name of the XML file in the "data" folder.
 *     details: Object containing the parameters of addEngineWithDetails,
 *              except for the engine name.  Alternative to xmlFileName.
 *   }
 */
var addTestEngines = async function(aItems) {
  if (!gDataUrl) {
    do_throw("useHttpServer must be called before addTestEngines.");
  }

  let engines = [];

  for (let item of aItems) {
    info("Adding engine: " + item.name);
    await new Promise((resolve, reject) => {
      Services.obs.addObserver(function obs(subject, topic, data) {
        try {
          let engine = subject.QueryInterface(Ci.nsISearchEngine);
          info("Observed " + data + " for " + engine.name);
          if (data != "engine-added" || engine.name != item.name) {
            return;
          }

          Services.obs.removeObserver(obs, "browser-search-engine-modified");
          engines.push(engine);
          resolve();
        } catch (ex) {
          reject(ex);
        }
      }, "browser-search-engine-modified");

      if (item.xmlFileName) {
        Services.search.addEngine(gDataUrl + item.xmlFileName, null, false);
      } else {
        Services.search.addEngineWithDetails(item.name, item.details);
      }
    });
  }

  return engines;
};

/**
 * Installs a test engine into the test profile.
 *
 * @returns {Array<SearchEngine>}
 */
function installTestEngine() {
  useHttpServer();
  return addTestEngines([{ name: kTestEngineName, xmlFileName: "engine.xml" }]);
}

async function asyncReInit({ awaitRegionFetch = false } = {}) {
  let promises = [SearchTestUtils.promiseSearchNotification("reinit-complete")];
  if (awaitRegionFetch) {
    promises.push(
      SearchTestUtils.promiseSearchNotification("ensure-known-region-done")
    );
  }

  Services.search.reInit(awaitRegionFetch);

  return Promise.all(promises);
}

// This "enum" from nsSearchService.js
const TELEMETRY_RESULT_ENUM = {
  SUCCESS: 0,
  SUCCESS_WITHOUT_DATA: 1,
  XHRTIMEOUT: 2,
  ERROR: 3,
};

/**
 * Checks the value of the SEARCH_SERVICE_COUNTRY_FETCH_RESULT probe.
 *
 * @param {string|null} aExpectedValue
 *   If a value from TELEMETRY_RESULT_ENUM, we expect to see this value
 *   recorded exactly once in the probe.  If |null|, we expect to see
 *   nothing recorded in the probe at all.
 */
function checkCountryResultTelemetry(aExpectedValue) {
  let histogram = Services.telemetry.getHistogramById(
    "SEARCH_SERVICE_COUNTRY_FETCH_RESULT"
  );
  let snapshot = histogram.snapshot();
  if (aExpectedValue != null) {
    equal(snapshot.values[aExpectedValue], 1);
  } else {
    deepEqual(snapshot.values, {});
  }
}

/**
 * Provides a basic set of remote settings for use in tests.
 */
async function setupRemoteSettings() {
  const settings = await RemoteSettings("hijack-blocklists");
  sinon.stub(settings, "get").returns([
    {
      id: "load-paths",
      matches: ["[other]addEngineWithDetails:searchignore@mozilla.com"],
      _status: "synced",
    },
    {
      id: "submission-urls",
      matches: ["ignore=true"],
      _status: "synced",
    },
  ]);
}

/**
 * Some tests might trigger initialisation which will trigger the search settings
 * update. We need to make sure we wait for that to finish before we exit, otherwise
 * it may cause shutdown issues.
 */
let updatePromise = SearchTestUtils.promiseSearchNotification(
  "settings-update-complete"
);

registerCleanupFunction(async () => {
  if (Services.search.isInitialized) {
    await updatePromise;
  }
});
