/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ 
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LightweightThemeManager.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);

const IGNORE_HISTOGRAM = "test::ignore_me";
const IGNORE_HISTOGRAM_TO_CLONE = "MEMORY_HEAP_ALLOCATED";
const IGNORE_CLONED_HISTOGRAM = "test::ignore_me_also";
const ADDON_NAME = "Telemetry test addon";
const ADDON_HISTOGRAM = "addon-histogram";
// Add some unicode characters here to ensure that sending them works correctly.
const FLASH_VERSION = "\u201c1.1.1.1\u201d";
const SHUTDOWN_TIME = 10000;
const FAILED_PROFILE_LOCK_ATTEMPTS = 2;

// Constants from prio.h for nsIFileOutputStream.init
const PR_WRONLY = 0x2;
const PR_CREATE_FILE = 0x8;
const PR_TRUNCATE = 0x20;
const RW_OWNER = 0600;

const NUMBER_OF_THREADS_TO_LAUNCH = 30;
let gNumberOfThreadsLaunched = 0;

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);

let gHttpServer = new HttpServer();
let gServerStarted = false;
let gRequestIterator = null;

function sendPing () {
  TelemetryPing.gatherStartup();
  if (gServerStarted) {
    return TelemetryPing.testPing("http://localhost:" + gHttpServer.identity.primaryPort);
  } else {
    return TelemetryPing.testPing("http://doesnotexist");
  }
}

function wrapWithExceptionHandler(f) {
  function wrapper(...args) {
    try {
      f(...args);
    } catch (ex if typeof(ex) == 'object') {
      dump("Caught exception: " + ex.message + "\n");
      dump(ex.stack);
      do_test_finished();
    }
  }
  return wrapper;
}

function registerPingHandler(handler) {
  gHttpServer.registerPrefixHandler("/submit/telemetry/",
				   wrapWithExceptionHandler(handler));
}

function setupTestData() {
  Telemetry.newHistogram(IGNORE_HISTOGRAM, "never", 1, 2, 3, Telemetry.HISTOGRAM_BOOLEAN);
  Telemetry.histogramFrom(IGNORE_CLONED_HISTOGRAM, IGNORE_HISTOGRAM_TO_CLONE);
  Services.startup.interrupted = true;
  Telemetry.registerAddonHistogram(ADDON_NAME, ADDON_HISTOGRAM, 1, 5, 6,
                                   Telemetry.HISTOGRAM_LINEAR);
  h1 = Telemetry.getAddonHistogram(ADDON_NAME, ADDON_HISTOGRAM);
  h1.add(1);
}

function getSavedHistogramsFile(basename) {
  let tmpDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let histogramsFile = tmpDir.clone();
  histogramsFile.append(basename);
  if (histogramsFile.exists()) {
    histogramsFile.remove(true);
  }
  do_register_cleanup(function () {
    try {
      histogramsFile.remove(true);
    } catch (e) {
    }
  });
  return histogramsFile;
}

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
    payload = decoder.decode(utf8string);
  } else {
    payload = decoder.decodeFromStream(s, s.available());
  }

  return payload;
}

function checkPayloadInfo(payload, reason) {
  // get rid of the non-deterministic field
  const expected_info = {
    OS: "XPCShell", 
    appID: "xpcshell@tests.mozilla.org", 
    appVersion: "1", 
    appName: "XPCShell", 
    appBuildID: "2007010101",
    platformBuildID: "2007010101",
    flashVersion: FLASH_VERSION
  };

  for (let f in expected_info) {
    do_check_eq(payload.info[f], expected_info[f]);
  }

  do_check_eq(payload.info.reason, reason);
  do_check_true("appUpdateChannel" in payload.info);
  do_check_true("locale" in payload.info);
  do_check_true("revision" in payload.info);
  do_check_true(payload.info.revision.startsWith("http"));

  try {
    // If we've not got nsIGfxInfoDebug, then this will throw and stop us doing
    // this test.
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);
    let isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
    let isOSX = ("nsILocalFileMac" in Components.interfaces);

    if (isWindows || isOSX) {
      do_check_true("adapterVendorID" in payload.info);
      do_check_true("adapterDeviceID" in payload.info);
    }
  }
  catch (x) {
  }
}

function checkPayload(request, reason, successfulPings) {
  let payload = decodeRequestPayload(request);
  // Take off ["","submit","telemetry"].
  let pathComponents = request.path.split("/").slice(3);

  checkPayloadInfo(payload, reason);
  do_check_eq(reason, pathComponents[1]);
  do_check_eq(request.getHeader("content-type"), "application/json; charset=UTF-8");
  do_check_true(payload.simpleMeasurements.uptime >= 0);
  do_check_true(payload.simpleMeasurements.startupInterrupted === 1);
  do_check_eq(payload.simpleMeasurements.shutdownDuration, SHUTDOWN_TIME);
  do_check_eq(payload.simpleMeasurements.savedPings, 1);
  do_check_true("maximalNumberOfConcurrentThreads" in payload.simpleMeasurements);
  do_check_true(payload.simpleMeasurements.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  do_check_eq(payload.simpleMeasurements.failedProfileLockCount,
              FAILED_PROFILE_LOCK_ATTEMPTS);
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let failedProfileLocksFile = profileDirectory.clone();
  failedProfileLocksFile.append("Telemetry.FailedProfileLocks.txt");
  do_check_true(!failedProfileLocksFile.exists());


  let isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
  if (isWindows) {
    do_check_true(payload.simpleMeasurements.startupSessionRestoreReadBytes > 0);
    do_check_true(payload.simpleMeasurements.startupSessionRestoreWriteBytes > 0);
  }

  const TELEMETRY_PING = "TELEMETRY_PING";
  const TELEMETRY_SUCCESS = "TELEMETRY_SUCCESS";
  const TELEMETRY_TEST_FLAG = "TELEMETRY_TEST_FLAG";
  const READ_SAVED_PING_SUCCESS = "READ_SAVED_PING_SUCCESS";
  do_check_true(TELEMETRY_PING in payload.histograms);
  do_check_true(READ_SAVED_PING_SUCCESS in payload.histograms);
  let rh = Telemetry.registeredHistograms([]);
  for (let name of rh) {
    if (/SQLITE/.test(name) && name in payload.histograms) {
      do_check_true(("STARTUP_" + name) in payload.histograms); 
    }
  }
  do_check_false(IGNORE_HISTOGRAM in payload.histograms);
  do_check_false(IGNORE_CLONED_HISTOGRAM in payload.histograms);

  // Flag histograms should automagically spring to life.
  const expected_flag = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 3,
    values: {0:1, 1:0},
    sum: 0,
    sum_squares_lo: 0,
    sum_squares_hi: 0
  };
  let flag = payload.histograms[TELEMETRY_TEST_FLAG];
  do_check_eq(uneval(flag), uneval(expected_flag));

  // There should be one successful report from the previous telemetry ping.
  const expected_tc = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 2,
    values: {0:1, 1:successfulPings, 2:0},
    sum: successfulPings,
    sum_squares_lo: successfulPings,
    sum_squares_hi: 0
  };
  let tc = payload.histograms[TELEMETRY_SUCCESS];
  do_check_eq(uneval(tc), uneval(expected_tc));

  let h = payload.histograms[READ_SAVED_PING_SUCCESS];
  do_check_eq(h.values[0], 1);

  // The ping should include data from memory reporters.  We can't check that
  // this data is correct, because we can't control the values returned by the
  // memory reporters.  But we can at least check that the data is there.
  //
  // It's important to check for the presence of reporters with a mix of units,
  // because TelemetryPing has separate logic for each one.  But we can't
  // currently check UNITS_COUNT_CUMULATIVE or UNITS_PERCENTAGE because
  // Telemetry doesn't touch a memory reporter with these units that's
  // available on all platforms.

  do_check_true('MEMORY_JS_GC_HEAP' in payload.histograms); // UNITS_BYTES
  do_check_true('MEMORY_JS_COMPARTMENTS_SYSTEM' in payload.histograms); // UNITS_COUNT

  // We should have included addon histograms.
  do_check_true("addonHistograms" in payload);
  do_check_true(ADDON_NAME in payload.addonHistograms);
  do_check_true(ADDON_HISTOGRAM in payload.addonHistograms[ADDON_NAME]);

  do_check_true(("mainThread" in payload.slowSQL) &&
                ("otherThreads" in payload.slowSQL));
}

function dummyTheme(id) {
  return {
    id: id,
    name: Math.random().toString(),
    headerURL: "http://lwttest.invalid/a.png",
    footerURL: "http://lwttest.invalid/b.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };
}

// A fake plugin host for testing flash version telemetry
let PluginHost = {
  getPluginTags: function(countRef) {
    let plugins = [{name: "Shockwave Flash", version: FLASH_VERSION}];
    countRef.value = plugins.length;
    return plugins;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIPluginHost)
     || iid.equals(Ci.nsISupports))
      return this;
  
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

let PluginHostFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return PluginHost.QueryInterface(iid);
  }
};

const PLUGINHOST_CONTRACTID = "@mozilla.org/plugin/host;1";
const PLUGINHOST_CID = Components.ID("{2329e6ea-1f15-4cbe-9ded-6e98e842de0e}");

function registerFakePluginHost() {
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(PLUGINHOST_CID, "Fake Plugin Host",
                            PLUGINHOST_CONTRACTID, PluginHostFactory);
}

function writeStringToFile(file, contents) {
  let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                .createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
	       RW_OWNER, ostream.DEFER_OPEN);
  ostream.write(contents, contents.length);
  ostream.QueryInterface(Ci.nsISafeOutputStream).finish();
  ostream.close();
}

function write_fake_shutdown_file() {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append("Telemetry.ShutdownTime.txt");
  let contents = "" + SHUTDOWN_TIME;
  writeStringToFile(file, contents);
}

function write_fake_failedprofilelocks_file() {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append("Telemetry.FailedProfileLocks.txt");
  let contents = "" + FAILED_PROFILE_LOCK_ATTEMPTS;
  writeStringToFile(file, contents);
}

function run_test() {
  do_test_pending();
  try {
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);
    gfxInfo.spoofVendorID("0xabcd");
    gfxInfo.spoofDeviceID("0x1234");
  } catch (x) {
    // If we can't test gfxInfo, that's fine, we'll note it later.
  }

  // Addon manager needs a profile directory
  do_get_profile();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Make it look like we've previously failed to lock a profile a couple times.
  write_fake_failedprofilelocks_file();

  // Make it look like we've shutdown before.
  write_fake_shutdown_file();

  let currentMaxNumberOfThreads = Telemetry.maximalNumberOfConcurrentThreads;
  do_check_true(currentMaxNumberOfThreads > 0);

  // Try to augment the maximal number of threads currently launched
  let threads = [];
  try {
    for (let i = 0; i < currentMaxNumberOfThreads + 10; ++i) {
      threads.push(Services.tm.newThread(0));
    }
  } catch (ex) {
    // If memory is too low, it is possible that not all threads will be launched.
  }
  gNumberOfThreadsLaunched = threads.length;

  do_check_true(Telemetry.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  do_register_cleanup(function() {
    threads.forEach(function(thread) {
      thread.shutdown();
    });
  });

  Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(actualTest));
}

function actualTest() {
  // try to make LightweightThemeManager do stuff
  let gInternalManager = Cc["@mozilla.org/addons/integration;1"]
                         .getService(Ci.nsIObserver)
                         .QueryInterface(Ci.nsITimerCallback);

  gInternalManager.observe(null, "addons-startup", null);
  LightweightThemeManager.currentTheme = dummyTheme("1234");

  // fake plugin host for consistent flash version data
  registerFakePluginHost();

  run_next_test();
}

// Ensures that expired histograms are not part of the payload.
add_task(function* test_expiredHistogram() {
  let histogram_id = "FOOBAR";
  let dummy = Telemetry.newHistogram(histogram_id, "30", 1, 2, 3, Telemetry.HISTOGRAM_EXPONENTIAL);

  dummy.add(1);

  do_check_eq(TelemetryPing.getPayload()["histograms"][histogram_id], undefined);
  do_check_eq(TelemetryPing.getPayload()["histograms"]["TELEMETRY_TEST_EXPIRED"], undefined);
});

// Checks that an invalid histogram file is deleted if TelemetryFile fails to parse it.
add_task(function* test_runInvalidJSON() {
  let histogramsFile = getSavedHistogramsFile("invalid-histograms.dat");

  writeStringToFile(histogramsFile, "this.is.invalid.JSON");
  do_check_true(histogramsFile.exists());

  yield TelemetryPing.testLoadHistograms(histogramsFile);
  do_check_false(histogramsFile.exists());
});

// Sends a ping to a non existing server.
add_task(function* test_noServerPing() {
  yield sendPing();
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(function* test_simplePing() {
  gHttpServer.start(-1);
  gServerStarted = true;
  gRequestIterator = Iterator(new Request());

  yield sendPing();
  decodeRequestPayload(yield gRequestIterator.next());
});

// Saves the current session histograms, reloads them, perfoms a ping
// and checks that the dummy http server received both the previously
// saved histograms and the new ones.
add_task(function* test_saveLoadPing() {
  let histogramsFile = getSavedHistogramsFile("saved-histograms.dat");

  setupTestData();
  yield TelemetryPing.testSaveHistograms(histogramsFile);
  yield TelemetryPing.testLoadHistograms(histogramsFile);
  yield sendPing();
  checkPayload((yield gRequestIterator.next()), "test-ping", 1);
  checkPayload((yield gRequestIterator.next()), "saved-session", 1);
});

// Checks that an expired histogram file is deleted when loaded.
add_task(function* test_runOldPingFile() {
  let histogramsFile = getSavedHistogramsFile("old-histograms.dat");

  yield TelemetryPing.testSaveHistograms(histogramsFile);
  do_check_true(histogramsFile.exists());
  let mtime = histogramsFile.lastModifiedTime;
  histogramsFile.lastModifiedTime = mtime - (14 * 24 * 60 * 60 * 1000 + 60000); // 14 days, 1m

  yield TelemetryPing.testLoadHistograms(histogramsFile);
  do_check_false(histogramsFile.exists());
});

add_task(function* stopServer(){
  gHttpServer.stop(do_test_finished);
});

// An iterable sequence of http requests
function Request() {
  let defers = [];
  let current = 0;

  function RequestIterator() {}

  // Returns a promise that resolves to the next http request
  RequestIterator.prototype.next = function() {
    let deferred = defers[current++];
    return deferred.promise;
  }

  this.__iterator__ = function(){
    return new RequestIterator();
  }

  registerPingHandler((request, response) => {
    let deferred = defers[defers.length - 1];
    defers.push(Promise.defer());
    deferred.resolve(request);
  });

  defers.push(Promise.defer());
}
