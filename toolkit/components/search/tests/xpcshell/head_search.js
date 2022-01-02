/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Region: "resource://gre/modules/Region.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  SearchSettings: "resource://gre/modules/SearchSettings.jsm",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
  SearchService: "resource://gre/modules/SearchService.jsm",
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
const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

SearchTestUtils.init(this);

const SETTINGS_FILENAME = "search.json.mozlz4";

// nsSearchService.js uses Services.appinfo.name to build a salt for a hash.
// eslint-disable-next-line mozilla/use-services
var XULRuntime = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime);

// Expand the amount of information available in error logs
Services.prefs.setBoolPref("browser.search.log", true);
Services.prefs.setBoolPref("browser.region.log", true);

Services.prefs.setBoolPref("browser.search.modernConfig", true);

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

// For tests, allow the settings to write sooner than it would do normally so that
// the tests that need to wait for it can run a bit faster.
SearchSettings.SETTNGS_INVALIDATION_DELAY = 250;

async function promiseSettingsData() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, SETTINGS_FILENAME);
  return JSON.parse(await IOUtils.readUTF8(path, { decompress: true }));
}

function promiseSaveSettingsData(data) {
  return IOUtils.write(
    OS.Path.join(OS.Constants.Path.profileDir, SETTINGS_FILENAME),
    new TextEncoder().encode(JSON.stringify(data)),
    { compress: true }
  );
}

async function promiseEngineMetadata() {
  let settings = await promiseSettingsData();
  let data = {};
  for (let engine of settings.engines) {
    data[engine._name] = engine._metaData;
  }
  return data;
}

async function promiseGlobalMetadata() {
  return (await promiseSettingsData()).metaData;
}

async function promiseSaveGlobalMetadata(globalData) {
  let data = await promiseSettingsData();
  data.metaData = globalData;
  await promiseSaveSettingsData(data);
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
 * Clean the profile of any settings file left from a previous run.
 *
 * @returns {boolean}
 *   Indicates if the settings file existed.
 */
function removeSettingsFile() {
  let file = do_get_profile().clone();
  file.append(SETTINGS_FILENAME);
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

const kTestEngineName = "Test search engine";

/**
 * Waits for the settings file to be saved.
 * @returns {Promise} Resolved when the settings file is saved.
 */
function promiseAfterSettings() {
  return SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
}

/**
 * Sets the home region, and waits for the search service to reload the engines.
 *
 * @param {string} region
 *   The region to set.
 */
async function promiseSetHomeRegion(region) {
  let promise = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion(region);
  await promise;
}

/**
 * Sets the requested/available locales and waits for the search service to
 * reload the engines.
 *
 * @param {string} locale
 *   The locale to set.
 */
async function promiseSetLocale(locale) {
  if (!Services.locale.availableLocales.includes(locale)) {
    throw new Error(
      `"${locale}" needs to be included in Services.locales.availableLocales at the start of the test.`
    );
  }

  let promise = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Services.locale.requestedLocales = [locale];
  await promise;
}

/**
 * Read a JSON file and return the JS object
 *
 * @param {nsIFile} file
 *   The file to read.
 * @returns {object}
 *   Returns the JSON object if the file was successfully read,
 *   false otherwise.
 */
async function readJSONFile(file) {
  return JSON.parse(await IOUtils.readUTF8(file.path));
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
        actualObj[prop]?.length,
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
 * @param {string} dir
 *   The test sub-directory to use for the engines.
 * @returns {HttpServer}
 *   The HttpServer object in case further customization is needed.
 */
function useHttpServer(dir = "data") {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  gDataUrl = `http://localhost:${httpServer.identity.primaryPort}/${dir}/`;
  registerCleanupFunction(async function cleanup_httpServer() {
    await new Promise(resolve => {
      httpServer.stop(resolve);
    });
  });
  return httpServer;
}

// This "enum" from nsSearchService.js
const TELEMETRY_RESULT_ENUM = {
  SUCCESS: 0,
  SUCCESS_WITHOUT_DATA: 1,
  TIMEOUT: 2,
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
 * Helper function that sets up a server and respnds to region
 * fetch requests.
 * @param {string} region
 *   The region that the server will respond with.
 * @param {Promise|null} waitToRespond
 *   A promise that the server will await on to delay responding
 *   to the request.
 */
function useCustomGeoServer(region, waitToRespond = Promise.resolve()) {
  let srv = useHttpServer();
  srv.registerPathHandler("/fetch_region", async (req, res) => {
    res.processAsync();
    await waitToRespond;
    res.setStatusLine("1.1", 200, "OK");
    res.write(JSON.stringify({ country_code: region }));
    res.finish();
  });

  Services.prefs.setCharPref(
    "browser.region.network.url",
    `http://localhost:${srv.identity.primaryPort}/fetch_region`
  );
}

/**
 * A simple observer to ensure we get only the expected notifications.
 */
class SearchObserver {
  constructor(expectedNotifications, returnEngineForNotification = false) {
    this.observer = this.observer.bind(this);
    this.deferred = PromiseUtils.defer();
    this.expectedNotifications = expectedNotifications;
    this.returnEngineForNotification = returnEngineForNotification;

    Services.obs.addObserver(this.observer, SearchUtils.TOPIC_ENGINE_MODIFIED);

    this.timeout = setTimeout(this.handleTimeout.bind(this), 1000);
  }

  get promise() {
    return this.deferred.promise;
  }

  handleTimeout() {
    this.deferred.reject(
      new Error(
        "Waiting for Notifications timed out, only received: " +
          this.expectedNotifications.join(",")
      )
    );
  }

  observer(subject, topic, data) {
    Assert.greater(
      this.expectedNotifications.length,
      0,
      "Should be expecting a notification"
    );
    Assert.equal(
      data,
      this.expectedNotifications[0],
      "Should have received the next expected notification"
    );

    if (
      this.returnEngineForNotification &&
      data == this.returnEngineForNotification
    ) {
      this.engineToReturn = subject.QueryInterface(Ci.nsISearchEngine);
    }

    this.expectedNotifications.shift();

    if (!this.expectedNotifications.length) {
      clearTimeout(this.timeout);
      delete this.timeout;
      this.deferred.resolve(this.engineToReturn);
      Services.obs.removeObserver(
        this.observer,
        SearchUtils.TOPIC_ENGINE_MODIFIED
      );
    }
  }
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

let consoleAllowList = [
  'property "localProfileDir" is non-configurable and can\'t be deleted',
  'property "profileDir" is non-configurable and can\'t be deleted',
];

let consoleListener = {
  observe(subject, topic, data) {
    let msg = subject.wrappedJSObject;
    let messageContents = msg.arguments[0]?.message || msg.arguments[0];
    if (
      msg.level == "error" &&
      !consoleAllowList.some(e => messageContents.includes(e))
    ) {
      Assert.ok(false, "Unexpected console message: " + messageContents);
    }
  },
};

Services.obs.addObserver(consoleListener, "console-api-log-event");

registerCleanupFunction(async () => {
  Services.obs.removeObserver(consoleListener, "console-api-log-event");
});
