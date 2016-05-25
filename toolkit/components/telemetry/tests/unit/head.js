/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var { classes: Cc, utils: Cu, interfaces: Ci, results: Cr } = Components;

Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/AppConstants.jsm");

const gIsWindows = AppConstants.platform == "win";
const gIsMac = AppConstants.platform == "macosx";
const gIsAndroid = AppConstants.platform == "android";
const gIsGonk = AppConstants.platform == "gonk";
const gIsLinux = AppConstants.platform == "linux";

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);

const MILLISECONDS_PER_MINUTE = 60 * 1000;
const MILLISECONDS_PER_HOUR = 60 * MILLISECONDS_PER_MINUTE;
const MILLISECONDS_PER_DAY = 24 * MILLISECONDS_PER_HOUR;

const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";

const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

var gGlobalScope = this;

const PingServer = {
  _httpServer: null,
  _started: false,
  _defers: [ PromiseUtils.defer() ],
  _currentDeferred: 0,

  get port() {
    return this._httpServer.identity.primaryPort;
  },

  get started() {
    return this._started;
  },

  registerPingHandler: function(handler) {
    const wrapped = wrapWithExceptionHandler(handler);
    this._httpServer.registerPrefixHandler("/submit/telemetry/", wrapped);
  },

  resetPingHandler: function() {
    this.registerPingHandler((request, response) => {
      let deferred = this._defers[this._defers.length - 1];
      this._defers.push(PromiseUtils.defer());
      deferred.resolve(request);
    });
  },

  start: function() {
    this._httpServer = new HttpServer();
    this._httpServer.start(-1);
    this._started = true;
    this.clearRequests();
    this.resetPingHandler();
  },

  stop: function() {
    return new Promise(resolve => {
      this._httpServer.stop(resolve);
      this._started = false;
    });
  },

  clearRequests: function() {
    this._defers = [ PromiseUtils.defer() ];
    this._currentDeferred = 0;
  },

  promiseNextRequest: function() {
    const deferred = this._defers[this._currentDeferred++];
    return deferred.promise;
  },

  promiseNextPing: function() {
    return this.promiseNextRequest().then(request => decodeRequestPayload(request));
  },

  promiseNextRequests: Task.async(function*(count) {
    let results = [];
    for (let i=0; i<count; ++i) {
      results.push(yield this.promiseNextRequest());
    }

    return results;
  }),

  promiseNextPings: function(count) {
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
  let decoder = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON)

  if (request.getHeader("content-encoding") == "gzip") {
    let observer = {
      buffer: "",
      onStreamComplete: function(loader, context, status, length, result) {
        this.buffer = String.fromCharCode.apply(this, result);
      }
    };

    let scs = Cc["@mozilla.org/streamConverters;1"]
              .getService(Ci.nsIStreamConverterService);
    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                  .createInstance(Ci.nsIStreamLoader);
    listener.init(observer);
    let converter = scs.asyncConvertData("gzip", "uncompressed",
                                         listener, null);
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, s, 0, s.available());
    converter.onStopRequest(null, null, null);
    let unicodeConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = "UTF-8";
    let utf8string = unicodeConverter.ConvertToUnicode(observer.buffer);
    utf8string += unicodeConverter.Finish();
    payload = JSON.parse(utf8string);
  } else {
    payload = decoder.decodeFromStream(s, s.available());
  }

  return payload;
}

function wrapWithExceptionHandler(f) {
  function wrapper(...args) {
    try {
      f(...args);
    } catch (ex) {
      if (typeof(ex) != 'object') {
        throw ex;
      }
      dump("Caught exception: " + ex.message + "\n");
      dump(ex.stack);
      do_test_finished();
    }
  }
  return wrapper;
}

function loadAddonManager(ID, name, version, platformVersion) {
  let ns = {};
  Cu.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo(ID, name, version, platformVersion);
  // As we're not running in application, we need to setup the features directory
  // used by system add-ons.
  const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"], true);
  registerDirectory("XREAppFeat", distroDir);
  startupManager();
}

var gAppInfo = null;

function createAppInfo(ID, name, version, platformVersion) {
  let tmp = {};
  Cu.import("resource://testing-common/AppInfo.jsm", tmp);
  tmp.updateAppInfo({
    ID, name, version, platformVersion,
    crashReporter: true,
  });
  gAppInfo = tmp.getAppInfo();
}

// Fake the timeout functions for the TelemetryScheduler.
function fakeSchedulerTimer(set, clear) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.setSchedulerTickTimeout = set;
  session.Policy.clearSchedulerTickTimeout = clear;
}

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
    Cu.import("resource://gre/modules/TelemetrySession.jsm"),
    Cu.import("resource://gre/modules/TelemetryEnvironment.jsm"),
    Cu.import("resource://gre/modules/TelemetryController.jsm"),
    Cu.import("resource://gre/modules/TelemetryStorage.jsm"),
    Cu.import("resource://gre/modules/TelemetrySend.jsm"),
    Cu.import("resource://gre/modules/TelemetryReportingPolicy.jsm"),
  ];

  for (let m of modules) {
    m.Policy.now = () => date;
  }

  return new Date(date);
}

function fakeMonotonicNow(ms) {
  const m = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  m.Policy.monotonicNow = () => ms;
  return ms;
}

// Fake the timeout functions for TelemetryController sending.
function fakePingSendTimer(set, clear) {
  let module = Cu.import("resource://gre/modules/TelemetrySend.jsm");
  let obj = Cu.cloneInto({set, clear}, module, {cloneFunctions:true});
  module.Policy.setSchedulerTickTimeout = obj.set;
  module.Policy.clearSchedulerTickTimeout = obj.clear;
}

function fakeMidnightPingFuzzingDelay(delayMs) {
  let module = Cu.import("resource://gre/modules/TelemetrySend.jsm");
  module.Policy.midnightPingFuzzingDelay = () => delayMs;
}

function fakeGeneratePingId(func) {
  let module = Cu.import("resource://gre/modules/TelemetryController.jsm");
  module.Policy.generatePingId = func;
}

function fakeCachedClientId(uuid) {
  let module = Cu.import("resource://gre/modules/TelemetryController.jsm");
  module.Policy.getCachedClientID = () => uuid;
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
  return promise.then(() => false, () => true);
}

// Generates a random string of at least a specific length.
function generateRandomString(length) {
  let string = "";

  while (string.length < length) {
    string += Math.random().toString(36);
  }

  return string.substring(0, length);
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
  let TelemetryEnvironment =
    Cu.import("resource://gre/modules/TelemetryEnvironment.jsm").TelemetryEnvironment;
  TelemetryEnvironment.testWatchPreferences(new Map());
}

if (runningInParent) {
  // Set logging preferences for all the tests.
  Services.prefs.setCharPref("toolkit.telemetry.log.level", "Trace");
  // Telemetry archiving should be on.
  Services.prefs.setBoolPref("toolkit.telemetry.archive.enabled", true);
  // Telemetry xpcshell tests cannot show the infobar.
  Services.prefs.setBoolPref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
  // FHR uploads should be enabled.
  Services.prefs.setBoolPref("datareporting.healthreport.uploadEnabled", true);

  fakePingSendTimer((callback, timeout) => {
    Services.tm.mainThread.dispatch(() => callback(), Ci.nsIThread.DISPATCH_NORMAL);
  },
  () => {});

  do_register_cleanup(() => TelemetrySend.shutdown());
}

TelemetryController.testInitLogging();

// Avoid timers interrupting test behavior.
fakeSchedulerTimer(() => {}, () => {});
// Make pind sending predictable.
fakeMidnightPingFuzzingDelay(0);
