/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://testing-common/AppInfo.jsm");
Cu.import("resource://testing-common/httpd.js");

const BROWSER_SEARCH_PREF = "browser.search.";
const NS_APP_SEARCH_DIR = "SrchPlugns";

const MODE_RDONLY = FileUtils.MODE_RDONLY;
const MODE_WRONLY = FileUtils.MODE_WRONLY;
const MODE_CREATE = FileUtils.MODE_CREATE;
const MODE_TRUNCATE = FileUtils.MODE_TRUNCATE;

const CACHE_FILENAME = "search.json.mozlz4";

// nsSearchService.js uses Services.appinfo.name to build a salt for a hash.
var XULRuntime = Components.classesByID["{95d89e3e-a169-41a3-8e56-719978e15b12}"]
                           .getService(Ci.nsIXULRuntime);

var isChild = XULRuntime.processType == XULRuntime.PROCESS_TYPE_CONTENT;

updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@test.mozilla.org",
  version: "5",
  platformVersion: "1.9",
  // mirror OS from the base impl as some of the "location" tests rely on it
  OS: XULRuntime.OS,
  // mirror processType from the base implementation
  extraProps: {
    processType: XULRuntime.processType,
  },
});

var gProfD;
if (!isChild) {
  // Need to create and register a profile folder.
  gProfD = do_get_profile();
}

function dumpn(text)
{
  dump("search test: " + text + "\n");
}

/**
 * Configure preferences to load engines from
 * chrome://testsearchplugin/locale/searchplugins/
 */
function configureToLoadJarEngines()
{
  let defaultBranch = Services.prefs.getDefaultBranch(null);

  let url = "chrome://testsearchplugin/locale/searchplugins/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url, null, null));

  // Ensure a test engine exists in the app dir anyway.
  let dir = Services.dirsvc.get(NS_APP_SEARCH_DIR, Ci.nsIFile);
  if (!dir.exists())
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_get_file("data/engine-app.xml").copyTo(dir, "app.xml");
}

/**
 * Fake the installation of an add-on in the profile, by creating the
 * directory and registering it with the directory service.
 */
function installAddonEngine(name = "engine-addon")
{
  const XRE_EXTENSIONS_DIR_LIST = "XREExtDL";
  const gProfD = do_get_profile().QueryInterface(Ci.nsILocalFile);

  let dir = gProfD.clone();
  dir.append("extensions");
  if (!dir.exists())
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  dir.append("search-engine@tests.mozilla.org");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/install.rdf").copyTo(dir, "install.rdf");
  let addonDir = dir.clone();
  dir.append("searchplugins");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_get_file("data/" + name + ".xml").copyTo(dir, "bug645970.xml");

  Services.dirsvc.registerProvider({
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider,
                                           Ci.nsIDirectoryServiceProvider2]),

    getFile: function (prop, persistant) {
      throw Cr.NS_ERROR_FAILURE;
    },

    getFiles: function (prop) {
      let result = [];

      switch (prop) {
      case XRE_EXTENSIONS_DIR_LIST:
        result.push(addonDir);
        break;
      default:
        throw Cr.NS_ERROR_FAILURE;
      }

      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),
        hasMoreElements: () => result.length > 0,
        getNext: () => result.shift()
      };
    }
  });
}

/**
 * Copy the engine-distribution.xml engine to a fake distribution
 * created in the profile, and registered with the directory service.
 */
function installDistributionEngine()
{
  const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

  const gProfD = do_get_profile().QueryInterface(Ci.nsILocalFile);

  let dir = gProfD.clone();
  dir.append("distribution");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  let distDir = dir.clone();

  dir.append("searchplugins");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  dir.append("common");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine-override.xml").copyTo(dir, "bug645970.xml");

  Services.dirsvc.registerProvider({
    getFile: function(aProp, aPersistent) {
      aPersistent.value = true;
      if (aProp == XRE_APP_DISTRIBUTION_DIR)
        return distDir.clone();
      return null;
    }
  });
}

/**
 * Clean the profile of any metadata files left from a previous run.
 */
function removeMetadata()
{
  let file = gProfD.clone();
  file.append("search-metadata.json");
  if (file.exists()) {
    file.remove(false);
  }

  file = gProfD.clone();
  file.append("search.sqlite");
  if (file.exists()) {
    file.remove(false);
  }
}

function getSearchMetadata()
{
  // Check that search-metadata.json has been created
  let metadata = gProfD.clone();
  metadata.append("search-metadata.json");
  do_check_true(metadata.exists());

  do_print("Parsing metadata");
  return readJSONFile(metadata);
}

function promiseCacheData() {
  return new Promise(resolve => Task.spawn(function* () {
    let path = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
    let bytes = yield OS.File.read(path, {compression: "lz4"});
    resolve(JSON.parse(new TextDecoder().decode(bytes)));
  }));
}

function promiseSaveCacheData(data) {
  return OS.File.writeAtomic(OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME),
                             new TextEncoder().encode(JSON.stringify(data)),
                             {compression: "lz4"});
}

function promiseEngineMetadata() {
  return new Promise(resolve => Task.spawn(function* () {
    let cache = yield promiseCacheData();
    let data = {};
    for (let engine of cache.engines) {
      data[engine._shortName] = engine._metaData;
    }
    resolve(data);
  }));
}

function promiseGlobalMetadata() {
  return new Promise(resolve => Task.spawn(function* () {
    let cache = yield promiseCacheData();
    resolve(cache.metaData);
  }));
}

function promiseSaveGlobalMetadata(globalData) {
  return new Promise(resolve => Task.spawn(function* () {
    let data = yield promiseCacheData();
    data.metaData = globalData;
    yield promiseSaveCacheData(data);
    resolve();
  }));
}

var forceExpiration = Task.async(function* () {
  let metadata = yield promiseGlobalMetadata();

  // Make the current geodefaults expire 1s ago.
  metadata.searchDefaultExpir = Date.now() - 1000;
  yield promiseSaveGlobalMetadata(metadata);
});

/**
 * Clean the profile of any cache file left from a previous run.
 * Returns a boolean indicating if the cache file existed.
 */
function removeCacheFile()
{
  let file = gProfD.clone();
  file.append(CACHE_FILENAME);
  if (file.exists()) {
    file.remove(false);
    return true;
  }
  return false;
}

/**
 * isUSTimezone taken from nsSearchService.js
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

  let UTCOffset = (new Date()).getTimezoneOffset();
  return UTCOffset >= 150 && UTCOffset <= 600;
}

const kDefaultenginenamePref = "browser.search.defaultenginename";
const kTestEngineName = "Test search engine";
const kLocalePref = "general.useragent.locale";

function getDefaultEngineName(isUS) {
  const nsIPLS = Ci.nsIPrefLocalizedString;
  // Copy the logic from nsSearchService
  let pref = kDefaultenginenamePref;
  if (isUS === undefined)
    isUS = Services.prefs.getCharPref(kLocalePref) == "en-US" && isUSTimezone();
  if (isUS) {
    pref += ".US";
  }
  return Services.prefs.getComplexValue(pref, nsIPLS).data;
}

/**
 * Waits for the cache file to be saved.
 * @return {Promise} Resolved when the cache file is saved.
 */
function promiseAfterCache() {
  return waitForSearchNotification("write-cache-to-disk-complete");
}

function parseJsonFromStream(aInputStream) {
  const json = Cc["@mozilla.org/dom/json;1"].createInstance(Components.interfaces.nsIJSON);
  const data = json.decodeFromStream(aInputStream, aInputStream.available());
  return data;
}

/**
 * Read a JSON file and return the JS object
 */
function readJSONFile(aFile) {
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].
               createInstance(Ci.nsIFileInputStream);
  try {
    stream.init(aFile, MODE_RDONLY, FileUtils.PERMS_FILE, 0);
    return parseJsonFromStream(stream, stream.available());
  } catch (ex) {
    dumpn("readJSONFile: Error reading JSON file: " + ex);
  } finally {
    stream.close();
  }
  return false;
}

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 */
function isSubObjectOf(expectedObj, actualObj) {
  for (let prop in expectedObj) {
    if (expectedObj[prop] instanceof Object) {
      do_check_eq(expectedObj[prop].length, actualObj[prop].length);
      isSubObjectOf(expectedObj[prop], actualObj[prop]);
    } else {
      if (expectedObj[prop] != actualObj[prop])
        do_print("comparing property " + prop);
      do_check_eq(expectedObj[prop], actualObj[prop]);
    }
  }
}

// Can't set prefs if we're running in a child process, but the search  service
// doesn't run in child processes anyways.
if (!isChild) {
  // Expand the amount of information available in error logs
  Services.prefs.setBoolPref("browser.search.log", true);

  // The geo-specific search tests assume certain prefs are already setup, which
  // might not be true when run in comm-central etc.  So create them here.
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
  Services.prefs.setIntPref("browser.search.geoip.timeout", 3000);
  // But still disable geoip lookups - tests that need it will re-configure this.
  Services.prefs.setCharPref("browser.search.geoip.url", "");
  // Also disable region defaults - tests using it will also re-configure it.
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref("geoSpecificDefaults.url", "");
}

/**
 * After useHttpServer() is called, this string contains the URL of the "data"
 * directory, including the final slash.
 */
var gDataUrl;

/**
 * Initializes the HTTP server and ensures that it is terminated when tests end.
 *
 * @return The HttpServer object in case further customization is needed.
 */
function useHttpServer() {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  gDataUrl = "http://localhost:" + httpServer.identity.primaryPort + "/data/";
  do_register_cleanup(() => httpServer.stop(() => {}));
  return httpServer;
}

/**
 * Adds test engines and returns a promise resolved when they are installed.
 *
 * The engines are added in the given order.
 *
 * @param aItems
 *        Array of objects with the following properties:
 *        {
 *          name: Engine name, used to wait for it to be loaded.
 *          xmlFileName: Name of the XML file in the "data" folder.
 *          details: Array containing the parameters of addEngineWithDetails,
 *                   except for the engine name.  Alternative to xmlFileName.
 *        }
 */
var addTestEngines = Task.async(function* (aItems) {
  if (!gDataUrl) {
    do_throw("useHttpServer must be called before addTestEngines.");
  }

  let engines = [];

  for (let item of aItems) {
    do_print("Adding engine: " + item.name);
    yield new Promise((resolve, reject) => {
      Services.obs.addObserver(function obs(subject, topic, data) {
        try {
          let engine = subject.QueryInterface(Ci.nsISearchEngine);
          do_print("Observed " + data + " for " + engine.name);
          if (data != "engine-added" || engine.name != item.name) {
            return;
          }

          Services.obs.removeObserver(obs, "browser-search-engine-modified");
          engines.push(engine);
          resolve();
        } catch (ex) {
          reject(ex);
        }
      }, "browser-search-engine-modified", false);

      if (item.xmlFileName) {
        Services.search.addEngine(gDataUrl + item.xmlFileName,
                                  null, null, false);
      } else {
        Services.search.addEngineWithDetails(item.name, ...item.details);
      }
    });
  }

  return engines;
});

/**
 * Installs a test engine into the test profile.
 */
function installTestEngine() {
  removeMetadata();
  removeCacheFile();

  do_check_false(Services.search.isInitialized);

  let engineDummyFile = gProfD.clone();
  engineDummyFile.append("searchplugins");
  engineDummyFile.append("test-search-engine.xml");
  let engineDir = engineDummyFile.parent;
  engineDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine.xml").copyTo(engineDir, "engine.xml");

  do_register_cleanup(function() {
    removeMetadata();
    removeCacheFile();
  });
}

/**
 * Set a localized preference on the default branch
 * @param aPrefName
 *        The name of the pref to set.
 */
function setLocalizedDefaultPref(aPrefName, aValue) {
  let value = "data:text/plain," + BROWSER_SEARCH_PREF + aPrefName + "=" + aValue;
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
          .setCharPref(aPrefName, value);
}


/**
 * Installs two test engines, sets them as default for US vs. general.
 */
function setUpGeoDefaults() {
  removeMetadata();
  removeCacheFile();

  do_check_false(Services.search.isInitialized);

  let engineDummyFile = gProfD.clone();
  engineDummyFile.append("searchplugins");
  engineDummyFile.append("test-search-engine.xml");
  let engineDir = engineDummyFile.parent;
  engineDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine.xml").copyTo(engineDir, "engine.xml");

  engineDummyFile = gProfD.clone();
  engineDummyFile.append("searchplugins");
  engineDummyFile.append("test-search-engine2.xml");

  do_get_file("data/engine2.xml").copyTo(engineDir, "engine2.xml");

  setLocalizedDefaultPref("defaultenginename",    "Test search engine");
  setLocalizedDefaultPref("defaultenginename.US", "A second test engine");

  do_register_cleanup(function() {
    removeMetadata();
    removeCacheFile();
  });
}

/**
 * Returns a promise that is resolved when an observer notification from the
 * search service fires with the specified data.
 *
 * @param aExpectedData
 *        The value the observer notification sends that causes us to resolve
 *        the promise.
 */
function waitForSearchNotification(aExpectedData) {
  return new Promise(resolve => {
    const SEARCH_SERVICE_TOPIC = "browser-search-service";
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      if (aData != aExpectedData)
        return;

      Services.obs.removeObserver(observer, SEARCH_SERVICE_TOPIC);
      resolve(aSubject);
    }, SEARCH_SERVICE_TOPIC, false);
  });
}

function asyncInit() {
  return new Promise(resolve => {
    Services.search.init(function() {
      do_check_true(Services.search.isInitialized);
      resolve();
    });
  });
}

function asyncReInit() {
  let promise = waitForSearchNotification("reinit-complete");

  Services.search.QueryInterface(Ci.nsIObserver)
          .observe(null, "nsPref:changed", kLocalePref);

  return promise;
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
 * @param aExpectedValue
 *        If a value from TELEMETRY_RESULT_ENUM, we expect to see this value
 *        recorded exactly once in the probe.  If |null|, we expect to see
 *        nothing recorded in the probe at all.
 */
function checkCountryResultTelemetry(aExpectedValue) {
  let histogram = Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_RESULT");
  let snapshot = histogram.snapshot();
  // The probe is declared with 8 values, but we get 9 back from .counts
  let expectedCounts = [0, 0, 0, 0, 0, 0, 0, 0, 0];
  if (aExpectedValue != null) {
    expectedCounts[aExpectedValue] = 1;
  }
  deepEqual(snapshot.counts, expectedCounts);
}
