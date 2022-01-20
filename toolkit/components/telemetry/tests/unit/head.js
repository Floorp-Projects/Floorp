/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryScheduler: "resource://gre/modules/TelemetryScheduler.jsm",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Log: "resource://gre/modules/Log.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  TelemetrySend: "resource://gre/modules/TelemetrySend.jsm",
  TelemetryStorage: "resource://gre/modules/TelemetryStorage.jsm",
});

const gIsWindows = AppConstants.platform == "win";
const gIsMac = AppConstants.platform == "macosx";
const gIsAndroid = AppConstants.platform == "android";
const gIsLinux = AppConstants.platform == "linux";

// Desktop Firefox, ie. not mobile Firefox or Thunderbird.
const gIsFirefox = AppConstants.MOZ_APP_NAME == "firefox";

const Telemetry = Services.telemetry;

const MILLISECONDS_PER_MINUTE = 60 * 1000;
const MILLISECONDS_PER_HOUR = 60 * MILLISECONDS_PER_MINUTE;
const MILLISECONDS_PER_DAY = 24 * MILLISECONDS_PER_HOUR;

const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

var gGlobalScope = this;

const PingServer = {
  _httpServer: null,
  _started: false,
  _defers: [PromiseUtils.defer()],
  _currentDeferred: 0,
  _logger: null,

  get port() {
    return this._httpServer.identity.primaryPort;
  },

  get started() {
    return this._started;
  },

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        "Toolkit.Telemetry",
        "PingServer::"
      );
    }

    return this._logger;
  },

  registerPingHandler(handler) {
    const wrapped = wrapWithExceptionHandler(handler);
    this._httpServer.registerPrefixHandler("/submit/telemetry/", wrapped);
  },

  resetPingHandler() {
    this.registerPingHandler((request, response) => {
      let r = request;
      this._log.trace(
        `defaultPingHandler() - ${r.method} ${r.scheme}://${r.host}:${r.port}${r.path}`
      );
      let deferred = this._defers[this._defers.length - 1];
      this._defers.push(PromiseUtils.defer());
      deferred.resolve(request);
    });
  },

  start() {
    this._httpServer = new HttpServer();
    this._httpServer.start(-1);
    this._started = true;
    this.clearRequests();
    this.resetPingHandler();
  },

  stop() {
    return new Promise(resolve => {
      this._httpServer.stop(resolve);
      this._started = false;
    });
  },

  clearRequests() {
    this._defers = [PromiseUtils.defer()];
    this._currentDeferred = 0;
  },

  promiseNextRequest() {
    const deferred = this._defers[this._currentDeferred++];
    // Send the ping to the consumer on the next tick, so that the completion gets
    // signaled to Telemetry.
    return new Promise(r =>
      Services.tm.dispatchToMainThread(() => r(deferred.promise))
    );
  },

  promiseNextPing() {
    return this.promiseNextRequest().then(request =>
      decodeRequestPayload(request)
    );
  },

  async promiseNextRequests(count) {
    let results = [];
    for (let i = 0; i < count; ++i) {
      results.push(await this.promiseNextRequest());
    }

    return results;
  },

  promiseNextPings(count) {
    return this.promiseNextRequests(count).then(requests => {
      return Array.from(requests, decodeRequestPayload);
    });
  },
};

/**
 * Decode the payload of an HTTP request into a ping.
 * @param {Object} request The data representing an HTTP request (nsIHttpRequest).
 * @return {Object} The decoded ping payload.
 */
function decodeRequestPayload(request) {
  let s = request.bodyInputStream;
  let payload = null;

  if (
    request.hasHeader("content-encoding") &&
    request.getHeader("content-encoding") == "gzip"
  ) {
    let observer = {
      buffer: "",
      onStreamComplete(loader, context, status, length, result) {
        // String.fromCharCode can only deal with 500,000 characters
        // at a time, so chunk the result into parts of that size.
        const chunkSize = 500000;
        for (let offset = 0; offset < result.length; offset += chunkSize) {
          this.buffer += String.fromCharCode.apply(
            String,
            result.slice(offset, offset + chunkSize)
          );
        }
      },
    };

    let scs = Cc["@mozilla.org/streamConverters;1"].getService(
      Ci.nsIStreamConverterService
    );
    let listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
      Ci.nsIStreamLoader
    );
    listener.init(observer);
    let converter = scs.asyncConvertData(
      "gzip",
      "uncompressed",
      listener,
      null
    );
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, s, 0, s.available());
    converter.onStopRequest(null, null, null);
    let unicodeConverter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = "UTF-8";
    let utf8string = unicodeConverter.ConvertToUnicode(observer.buffer);
    utf8string += unicodeConverter.Finish();
    payload = JSON.parse(utf8string);
  } else {
    let bytes = NetUtil.readInputStream(s, s.available());
    payload = JSON.parse(new TextDecoder().decode(bytes));
  }

  if (payload && "clientId" in payload) {
    // Check for canary value
    Assert.notEqual(
      TelemetryUtils.knownClientID,
      payload.clientId,
      `Known clientId shouldn't appear in a "${payload.type}" ping on the server.`
    );
  }

  return payload;
}

function checkPingFormat(aPing, aType, aHasClientId, aHasEnvironment) {
  const APP_VERSION = "1";
  const APP_NAME = "XPCShell";
  const PING_FORMAT_VERSION = 4;
  const PLATFORM_VERSION = "1.9.2";
  const MANDATORY_PING_FIELDS = [
    "type",
    "id",
    "creationDate",
    "version",
    "application",
    "payload",
  ];

  const APPLICATION_TEST_DATA = {
    buildId: gAppInfo.appBuildID,
    name: APP_NAME,
    version: APP_VERSION,
    displayVersion: AppConstants.MOZ_APP_VERSION_DISPLAY,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  // Check that the ping contains all the mandatory fields.
  for (let f of MANDATORY_PING_FIELDS) {
    Assert.ok(f in aPing, f + " must be available.");
  }

  Assert.equal(aPing.type, aType, "The ping must have the correct type.");
  Assert.equal(
    aPing.version,
    PING_FORMAT_VERSION,
    "The ping must have the correct version."
  );

  // Test the application section.
  for (let f in APPLICATION_TEST_DATA) {
    Assert.equal(
      aPing.application[f],
      APPLICATION_TEST_DATA[f],
      f + " must have the correct value."
    );
  }

  // We can't check the values for channel and architecture. Just make
  // sure they are in.
  Assert.ok(
    "architecture" in aPing.application,
    "The application section must have an architecture field."
  );
  Assert.ok(
    "channel" in aPing.application,
    "The application section must have a channel field."
  );

  // Check the clientId and environment fields, as needed.
  Assert.equal("clientId" in aPing, aHasClientId);
  Assert.equal("environment" in aPing, aHasEnvironment);
}

function wrapWithExceptionHandler(f) {
  function wrapper(...args) {
    try {
      f(...args);
    } catch (ex) {
      if (typeof ex != "object") {
        throw ex;
      }
      dump("Caught exception: " + ex.message + "\n");
      dump(ex.stack);
      do_test_finished();
    }
  }
  return wrapper;
}

async function loadAddonManager(...args) {
  AddonTestUtils.init(gGlobalScope);
  AddonTestUtils.overrideCertDB();
  createAppInfo(...args);

  // As we're not running in application, we need to setup the features directory
  // used by system add-ons.
  const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"], true);
  AddonTestUtils.registerDirectory("XREAppFeat", distroDir);
  await AddonTestUtils.overrideBuiltIns({
    system: ["tel-system-xpi@tests.mozilla.org"],
  });
  return AddonTestUtils.promiseStartupManager();
}

function finishAddonManagerStartup() {
  Services.obs.notifyObservers(null, "test-load-xpi-database");
}

var gAppInfo = null;

function createAppInfo(
  ID = "xpcshell@tests.mozilla.org",
  name = "XPCShell",
  version = "1.0",
  platformVersion = "1.0"
) {
  AddonTestUtils.createAppInfo(ID, name, version, platformVersion);
  gAppInfo = AddonTestUtils.appInfo;
}

// Fake the timeout functions for the TelemetryScheduler.
function fakeSchedulerTimer(set, clear) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryScheduler.jsm"
  );
  Policy.setSchedulerTickTimeout = set;
  Policy.clearSchedulerTickTimeout = clear;
}

/* global TelemetrySession:false, TelemetryEnvironment:false, TelemetryController:false,
          TelemetryStorage:false, TelemetrySend:false, TelemetryReportingPolicy:false
 */

/**
 * Fake the current date.
 * This passes all received arguments to a new Date constructor and
 * uses the resulting date to fake the time in Telemetry modules.
 *
 * @return Date The new faked date.
 */
function fakeNow(...args) {
  const date = new Date(...args);
  const modules = [
    ChromeUtils.import("resource://gre/modules/TelemetrySession.jsm"),
    ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm"),
    ChromeUtils.import("resource://gre/modules/TelemetryControllerParent.jsm"),
    ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm"),
    ChromeUtils.import("resource://gre/modules/TelemetrySend.jsm"),
    ChromeUtils.import("resource://gre/modules/TelemetryReportingPolicy.jsm"),
    ChromeUtils.import("resource://gre/modules/TelemetryScheduler.jsm"),
  ];

  for (let m of modules) {
    m.Policy.now = () => date;
  }

  return new Date(date);
}

function fakeMonotonicNow(ms) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetrySession.jsm"
  );
  Policy.monotonicNow = () => ms;
  return ms;
}

// Fake the timeout functions for TelemetryController sending.
function fakePingSendTimer(set, clear) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm"
  );
  let obj = Cu.cloneInto({ set, clear }, TelemetrySend, {
    cloneFunctions: true,
  });
  Policy.setSchedulerTickTimeout = obj.set;
  Policy.clearSchedulerTickTimeout = obj.clear;
}

function fakeMidnightPingFuzzingDelay(delayMs) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm"
  );
  Policy.midnightPingFuzzingDelay = () => delayMs;
}

function fakeGeneratePingId(func) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryControllerParent.jsm"
  );
  Policy.generatePingId = func;
}

function fakeCachedClientId(uuid) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryControllerParent.jsm"
  );
  Policy.getCachedClientID = () => uuid;
}

// Fake the gzip compression for the next ping to be sent out
// and immediately reset to the original function.
function fakeGzipCompressStringForNextPing(length) {
  const { Policy, gzipCompressString } = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm"
  );
  let largePayload = generateString(length);
  Policy.gzipCompressString = data => {
    Policy.gzipCompressString = gzipCompressString;
    return largePayload;
  };
}

function fakeIntlReady() {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryEnvironment.jsm"
  );
  Policy._intlLoaded = true;
  // Dispatch the observer event in case the promise has been registered already.
  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
}

// Override the uninstall ping file names
function fakeUninstallPingPath(aPathFcn) {
  const { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryStorage.jsm"
  );
  Policy.getUninstallPingPath =
    aPathFcn ||
    (id => ({
      directory: new FileUtils.File(OS.Constants.Path.profileDir),
      file: `uninstall_ping_0123456789ABCDEF_${id}.json`,
    }));
}

// Return a date that is |offset| ms in the future from |date|.
function futureDate(date, offset) {
  return new Date(date.getTime() + offset);
}

function truncateToDays(aMsec) {
  return Math.floor(aMsec / MILLISECONDS_PER_DAY);
}

// Returns a promise that resolves to true when the passed promise rejects,
// false otherwise.
function promiseRejects(promise) {
  return promise.then(
    () => false,
    () => true
  );
}

// Generates a random string of at least a specific length.
function generateRandomString(length) {
  let string = "";

  while (string.length < length) {
    string += Math.random().toString(36);
  }

  return string.substring(0, length);
}

function generateString(length) {
  return new Array(length + 1).join("a");
}

// Short-hand for retrieving the histogram with that id.
function getHistogram(histogramId) {
  return Telemetry.getHistogramById(histogramId);
}

// Short-hand for retrieving the snapshot of the Histogram with that id.
function getSnapshot(histogramId) {
  return Telemetry.getHistogramById(histogramId).snapshot();
}

// Helper for setting an empty list of Environment preferences to watch.
function setEmptyPrefWatchlist() {
  const { TelemetryEnvironment } = ChromeUtils.import(
    "resource://gre/modules/TelemetryEnvironment.jsm"
  );
  return TelemetryEnvironment.onInitialized().then(() =>
    TelemetryEnvironment.testWatchPreferences(new Map())
  );
}

if (runningInParent) {
  // Set logging preferences for all the tests.
  Services.prefs.setCharPref("toolkit.telemetry.log.level", "Trace");
  // Telemetry archiving should be on.
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.ArchiveEnabled, true);
  // Telemetry xpcshell tests cannot show the infobar.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.BypassNotification,
    true
  );
  // FHR uploads should be enabled.
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);
  // Many tests expect the shutdown and the new-profile to not be sent on shutdown
  // and will fail if receive an unexpected ping. Let's globally disable these features:
  // the relevant tests will enable these prefs when needed.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.ShutdownPingSender,
    false
  );
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.ShutdownPingSenderFirstSession,
    false
  );
  Services.prefs.setBoolPref("toolkit.telemetry.newProfilePing.enabled", false);
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.FirstShutdownPingEnabled,
    false
  );
  // Turn off Health Ping submission.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.HealthPingEnabled,
    false
  );

  // Speed up child process accumulations
  Services.prefs.setIntPref(TelemetryUtils.Preferences.IPCBatchTimeout, 10);

  // Non-unified Telemetry (e.g. Fennec on Android) needs the preference to be set
  // in order to enable Telemetry.
  if (Services.prefs.getBoolPref(TelemetryUtils.Preferences.Unified, false)) {
    Services.prefs.setBoolPref(
      TelemetryUtils.Preferences.OverridePreRelease,
      true
    );
  } else {
    Services.prefs.setBoolPref(
      TelemetryUtils.Preferences.TelemetryEnabled,
      true
    );
  }

  fakePingSendTimer(
    (callback, timeout) => {
      Services.tm.dispatchToMainThread(() => callback());
    },
    () => {}
  );

  // This gets imported via fakeNow();
  registerCleanupFunction(() => TelemetrySend.shutdown());
}

TelemetryController.testInitLogging();

// Avoid timers interrupting test behavior.
fakeSchedulerTimer(
  () => {},
  () => {}
);
// Make pind sending predictable.
fakeMidnightPingFuzzingDelay(0);

// Avoid using the directory service, which is not registered in some tests.
fakeUninstallPingPath();

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

const DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC =
  "distribution-customization-complete";

const PLUGIN2_NAME = "Quicktime";
const PLUGIN2_DESC = "A mock Quicktime plugin";
const PLUGIN2_VERSION = "2.3";
//
// system add-ons are enabled at startup, so record date when the test starts
const SYSTEM_ADDON_INSTALL_DATE = Date.now();
