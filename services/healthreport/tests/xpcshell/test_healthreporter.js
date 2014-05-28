/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
let bsp = Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");
Cu.import("resource://gre/modules/services/datareporting/policy.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/services-common/bagheeraserver.js");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");
Cu.import("resource://testing-common/services/healthreport/utils.jsm");
Cu.import("resource://testing-common/AppData.jsm");


const DUMMY_URI = "http://localhost:62013/";
const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

const HealthReporterState = bsp.HealthReporterState;


function defineNow(policy, now) {
  print("Adjusting fake system clock to " + now);
  Object.defineProperty(policy, "now", {
    value: function customNow() {
      return now;
    },
    writable: true,
  });
}

function getReporter(name, uri, inspected) {
  return Task.spawn(function init() {
    let reporter = getHealthReporter(name, uri, inspected);
    yield reporter.init();

    yield reporter._providerManager.registerProviderFromType(
      HealthReportProvider);

    throw new Task.Result(reporter);
  });
}

function getReporterAndServer(name, namespace="test") {
  return Task.spawn(function get() {
    let server = new BagheeraServer();
    server.createNamespace(namespace);
    server.start();

    let reporter = yield getReporter(name, server.serverURI);
    reporter.serverNamespace = namespace;

    throw new Task.Result([reporter, server]);
  });
}

function shutdownServer(server) {
  let deferred = Promise.defer();
  server.stop(deferred.resolve.bind(deferred));

  return deferred.promise;
}

function getHealthReportProviderValues(reporter, day=null) {
  return Task.spawn(function getValues() {
    let p = reporter.getProvider("org.mozilla.healthreport");
    do_check_neq(p, null);
    let m = p.getMeasurement("submissions", 2);
    do_check_neq(m, null);

    let data = yield reporter._storage.getMeasurementValues(m.id);
    if (!day) {
      throw new Task.Result(data);
    }

    do_check_true(data.days.hasDay(day));
    let serializer = m.serializer(m.SERIALIZE_JSON)
    let json = serializer.daily(data.days.getDay(day));
    do_check_eq(json._v, 2);

    throw new Task.Result(json);
  });
}

function run_test() {
  run_next_test();
}

// run_test() needs to finish synchronously, so we do async init here.
add_task(function test_init() {
  yield makeFakeAppDir();
});

add_task(function test_constructor() {
  let reporter = yield getReporter("constructor");
  try {
    do_check_eq(reporter.lastPingDate.getTime(), 0);
    do_check_null(reporter.lastSubmitID);
    do_check_eq(typeof(reporter._state), "object");
    do_check_eq(reporter._state.lastPingDate.getTime(), 0);
    do_check_eq(reporter._state.remoteIDs.length, 0);
    do_check_eq(reporter._state.clientIDVersion, 1);
    do_check_neq(reporter._state.clientID, null);

    let failed = false;
    try {
      new HealthReporter("foo.bar");
    } catch (ex) {
      failed = true;
      do_check_true(ex.message.startsWith("Branch must end"));
    } finally {
      do_check_true(failed);
      failed = false;
    }
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_shutdown_normal() {
  let reporter = yield getReporter("shutdown_normal");

  // We can't send "quit-application" notification because the xpcshell runner
  // will shut down!
  reporter._initiateShutdown();
  yield reporter._promiseShutdown;
});

add_task(function test_shutdown_storage_in_progress() {
  let reporter = yield getHealthReporter("shutdown_storage_in_progress", DUMMY_URI, true);

  reporter.onStorageCreated = function () {
    print("Faking shutdown during storage initialization.");
    reporter._initiateShutdown();
  };

  reporter.init();

  yield reporter._promiseShutdown;
  do_check_eq(reporter.providerManagerShutdownCount, 0);
  do_check_eq(reporter.storageCloseCount, 1);
});

// Ensure that a shutdown triggered while provider manager is initializing
// results in shutdown and storage closure.
add_task(function test_shutdown_provider_manager_in_progress() {
  let reporter = yield getHealthReporter("shutdown_provider_manager_in_progress",
                                         DUMMY_URI, true);

  reporter.onProviderManagerInitialized = function () {
    print("Faking shutdown during provider manager initialization.");
    reporter._initiateShutdown();
  };

  reporter.init();

  // This will hang if shutdown logic is busted.
  yield reporter._promiseShutdown;
  do_check_eq(reporter.providerManagerShutdownCount, 1);
  do_check_eq(reporter.storageCloseCount, 1);
});

// Simulates an error during provider manager initialization and verifies we shut down.
add_task(function test_shutdown_when_provider_manager_errors() {
  let reporter = yield getHealthReporter("shutdown_when_provider_manager_errors",
                                       DUMMY_URI, true);

  let error = new Error("Fake error during provider manager initialization.");
  reporter.onInitializeProviderManagerFinished = function () {
    print("Throwing fake error.");
    throw error;
  };

  try {
    yield reporter.init();
    do_throw("The error was not reported by init()");
  } catch (ex if ex == error) {
    do_print("The error was reported by init()");
  }

  // This will hang if shutdown logic is busted.
  yield reporter._promiseShutdown;
  do_check_eq(reporter.providerManagerShutdownCount, 1);
  do_check_eq(reporter.storageCloseCount, 1);
});

// Pull-only providers are only initialized at collect time.
add_task(function test_pull_only_providers() {
  const category = "healthreporter-constant-only";

  let cm = Cc["@mozilla.org/categorymanager;1"]
             .getService(Ci.nsICategoryManager);
  cm.addCategoryEntry(category, "DummyProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);
  cm.addCategoryEntry(category, "DummyConstantProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);

  let reporter = yield getReporter("constant_only_providers");
  try {
    let initCount = reporter._providerManager.providers.length;
    yield reporter._providerManager.registerProvidersFromCategoryManager(category);
    do_check_eq(reporter._providerManager._providers.size, initCount + 1);
    do_check_true(reporter._storage.hasProvider("DummyProvider"));
    do_check_false(reporter._storage.hasProvider("DummyConstantProvider"));
    do_check_neq(reporter.getProvider("DummyProvider"), null);
    do_check_null(reporter.getProvider("DummyConstantProvider"));

    yield reporter.collectMeasurements();

    do_check_eq(reporter._providerManager._providers.size, initCount + 1);
    do_check_true(reporter._storage.hasProvider("DummyConstantProvider"));

    let mID = reporter._storage.measurementID("DummyConstantProvider", "DummyMeasurement", 1);
    let values = yield reporter._storage.getMeasurementValues(mID);
    do_check_true(values.singular.size > 0);
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_collect_daily() {
  let reporter = yield getReporter("collect_daily");

  try {
    let now = new Date();
    let provider = new DummyProvider();
    yield reporter._providerManager.registerProvider(provider);
    yield reporter.collectMeasurements();

    do_check_eq(provider.collectConstantCount, 1);
    do_check_eq(provider.collectDailyCount, 1);

    yield reporter.collectMeasurements();
    do_check_eq(provider.collectConstantCount, 1);
    do_check_eq(provider.collectDailyCount, 1);

    yield reporter.collectMeasurements();
    do_check_eq(provider.collectDailyCount, 1); // Too soon.

    reporter._lastDailyDate = now.getTime() - MILLISECONDS_PER_DAY - 1;
    yield reporter.collectMeasurements();
    do_check_eq(provider.collectDailyCount, 2);

    reporter._lastDailyDate = null;
    yield reporter.collectMeasurements();
    do_check_eq(provider.collectDailyCount, 3);
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_remove_old_lastpayload() {
  let reporter = getHealthReporter("remove-old-lastpayload");
  let lastPayloadPath = reporter._state._lastPayloadPath;
  let paths = [lastPayloadPath, lastPayloadPath + ".tmp"];
  let createFiles = function () {
    return Task.spawn(function createFiles() {
      for (let path of paths) {
        yield OS.File.writeAtomic(path, "delete-me", {tmpPath: path + ".tmp"});
        do_check_true(yield OS.File.exists(path));
      }
    });
  };
  try {
    do_check_true(!reporter._state.removedOutdatedLastpayload);
    yield createFiles();
    yield reporter.init();
    for (let path of paths) {
      do_check_false(yield OS.File.exists(path));
    }
    yield reporter._state.save();
    yield reporter._shutdown();

    let o = yield CommonUtils.readJSON(reporter._state._filename);
    do_check_true(o.removedOutdatedLastpayload);

    yield createFiles();
    reporter = getHealthReporter("remove-old-lastpayload");
    yield reporter.init();
    for (let path of paths) {
      do_check_true(yield OS.File.exists(path));
    }
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_json_payload_simple() {
  let reporter = yield getReporter("json_payload_simple");

  let clientID = reporter._state.clientID;
  do_check_neq(clientID, null);

  try {
    let now = new Date();
    let payload = yield reporter.getJSONPayload();
    do_check_eq(typeof payload, "string");
    let original = JSON.parse(payload);

    do_check_eq(original.version, 2);
    do_check_eq(original.thisPingDate, reporter._formatDate(now));
    do_check_eq(original.clientID, clientID);
    do_check_eq(original.clientIDVersion, reporter._state.clientIDVersion);
    do_check_eq(original.clientIDVersion, 1);
    do_check_eq(Object.keys(original.data.last).length, 0);
    do_check_eq(Object.keys(original.data.days).length, 0);
    do_check_false("notInitialized" in original);

    yield reporter._state.setLastPingDate(
      new Date(now.getTime() - 24 * 60 * 60 * 1000 - 10));

    original = JSON.parse(yield reporter.getJSONPayload());
    do_check_eq(original.lastPingDate, reporter._formatDate(reporter.lastPingDate));
    do_check_eq(original.clientID, clientID);

    // This could fail if we cross UTC day boundaries at the exact instance the
    // test is executed. Let's tempt fate.
    do_check_eq(original.thisPingDate, reporter._formatDate(now));

    payload = yield reporter.getJSONPayload(true);
    do_check_eq(typeof payload, "object");
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_json_payload_dummy_provider() {
  let reporter = yield getReporter("json_payload_dummy_provider");

  try {
    yield reporter._providerManager.registerProvider(new DummyProvider());
    yield reporter.collectMeasurements();
    let payload = yield reporter.getJSONPayload();
    print(payload);
    let o = JSON.parse(payload);

    let name = "DummyProvider.DummyMeasurement";
    do_check_eq(Object.keys(o.data.last).length, 1);
    do_check_true(name in o.data.last);
    do_check_eq(o.data.last[name]._v, 1);
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_collect_and_obtain_json_payload() {
  let reporter = yield getReporter("collect_and_obtain_json_payload");

  try {
    yield reporter._providerManager.registerProvider(new DummyProvider());
    let payload = yield reporter.collectAndObtainJSONPayload();
    do_check_eq(typeof payload, "string");

    let o = JSON.parse(payload);
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);

    payload = yield reporter.collectAndObtainJSONPayload(true);
    do_check_eq(typeof payload, "object");
  } finally {
    yield reporter._shutdown();
  }
});

// Ensure constant-only providers make their way into the JSON payload.
add_task(function test_constant_only_providers_in_json_payload() {
  const category = "healthreporter-constant-only-in-payload";

  let cm = Cc["@mozilla.org/categorymanager;1"]
             .getService(Ci.nsICategoryManager);
  cm.addCategoryEntry(category, "DummyProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);
  cm.addCategoryEntry(category, "DummyConstantProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);

  let reporter = yield getReporter("constant_only_providers_in_json_payload");
  try {
    let initCount = reporter._providerManager.providers.length;
    yield reporter._providerManager.registerProvidersFromCategoryManager(category);

    let payload = yield reporter.collectAndObtainJSONPayload();
    let o = JSON.parse(payload);
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);
    do_check_true("DummyConstantProvider.DummyMeasurement" in o.data.last);

    let providers = reporter._providerManager.providers;
    do_check_eq(providers.length, initCount + 1);

    // Do it again for good measure.
    payload = yield reporter.collectAndObtainJSONPayload();
    o = JSON.parse(payload);
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);
    do_check_true("DummyConstantProvider.DummyMeasurement" in o.data.last);

    providers = reporter._providerManager.providers;
    do_check_eq(providers.length, initCount + 1);

    // Ensure throwing getJSONPayload is handled properly.
    Object.defineProperty(reporter, "_getJSONPayload", {
      value: function () {
        throw new Error("Silly error.");
      },
    });

    let deferred = Promise.defer();

    reporter.collectAndObtainJSONPayload().then(do_throw, function onError() {
      providers = reporter._providerManager.providers;
      do_check_eq(providers.length, initCount + 1);
      deferred.resolve();
    });

    yield deferred.promise;
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_json_payload_multiple_days() {
  let reporter = yield getReporter("json_payload_multiple_days");

  try {
    let provider = new DummyProvider();
    yield reporter._providerManager.registerProvider(provider);

    let now = new Date();

    let m = provider.getMeasurement("DummyMeasurement", 1);
    for (let i = 0; i < 200; i++) {
      let date = new Date(now.getTime() - i * MILLISECONDS_PER_DAY);
      yield m.incrementDailyCounter("daily-counter", date);
    }

    // This test could fail if we cross a UTC day boundary when running. So,
    // we ensure this doesn't occur.
    Object.defineProperty(reporter, "_now", {
      value: function () {
        return now;
      },
    });

    let payload = yield reporter.getJSONPayload();
    print(payload);
    let o = JSON.parse(payload);

    do_check_eq(Object.keys(o.data.days).length, 180);
    let today = reporter._formatDate(now);
    do_check_true(today in o.data.days);
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_json_payload_newer_version_overwrites() {
  let reporter = yield getReporter("json_payload_newer_version_overwrites");

  try {
    let now = new Date();
    // Instead of hacking up the internals to ensure consistent order in Map
    // iteration (which would be difficult), we instead opt to generate a lot
    // of measurements of different versions and verify their iterable order
    // is not increasing.
    let versions = [1, 6, 3, 9, 2, 3, 7, 4, 10, 8];
    let protos = [];
    for (let version of versions) {
      let m = function () {
        Metrics.Measurement.call(this);
      };
      m.prototype = {
        __proto__: DummyMeasurement.prototype,
        name: "DummyMeasurement",
        version: version,
      };

      protos.push(m);
    }

    let ctor = function () {
      Metrics.Provider.call(this);
    };
    ctor.prototype = {
      __proto__: DummyProvider.prototype,

      name: "MultiMeasurementProvider",
      measurementTypes: protos,
    };

    let provider = new ctor();

    yield reporter._providerManager.registerProvider(provider);

    let haveUnordered = false;
    let last = -1;
    let highestVersion = -1;
    for (let [key, measurement] of provider.measurements) {
      yield measurement.setDailyLastNumeric("daily-last-numeric",
                                            measurement.version, now);
      yield measurement.setLastNumeric("last-numeric",
                                       measurement.version, now);

      if (measurement.version > highestVersion) {
        highestVersion = measurement.version;
      }

      if (measurement.version < last) {
        haveUnordered = true;
      }

      last = measurement.version;
    }

    // Ensure Map traversal isn't ordered. If this ever fails, then we'll need
    // to monkeypatch.
    do_check_true(haveUnordered);

    let payload = yield reporter.getJSONPayload();
    let o = JSON.parse(payload);
    do_check_true("MultiMeasurementProvider.DummyMeasurement" in o.data.last);
    do_check_eq(o.data.last["MultiMeasurementProvider.DummyMeasurement"]._v, highestVersion);

    let day = reporter._formatDate(now);
    do_check_true(day in o.data.days);
    do_check_true("MultiMeasurementProvider.DummyMeasurement" in o.data.days[day]);
    do_check_eq(o.data.days[day]["MultiMeasurementProvider.DummyMeasurement"]._v, highestVersion);

  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_idle_daily() {
  let reporter = yield getReporter("idle_daily");
  try {
    let provider = new DummyProvider();
    yield reporter._providerManager.registerProvider(provider);

    let now = new Date();
    let m = provider.getMeasurement("DummyMeasurement", 1);
    for (let i = 0; i < 200; i++) {
      let date = new Date(now.getTime() - i * MILLISECONDS_PER_DAY);
      yield m.incrementDailyCounter("daily-counter", date);
    }

    let values = yield m.getValues();
    do_check_eq(values.days.size, 200);

    Services.obs.notifyObservers(null, "idle-daily", null);

    values = yield m.getValues();
    do_check_eq(values.days.size, 180);
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_data_submission_transport_failure() {
  let reporter = yield getReporter("data_submission_transport_failure");
  try {
    reporter.serverURI = DUMMY_URI;
    reporter.serverNamespace = "test00";

    let deferred = Promise.defer();
    let request = new DataSubmissionRequest(deferred, new Date(Date.now + 30000));
    reporter.requestDataUpload(request);

    yield deferred.promise;
    do_check_eq(request.state, request.SUBMISSION_FAILURE_SOFT);

    let data = yield getHealthReportProviderValues(reporter, new Date());
    do_check_eq(data.firstDocumentUploadAttempt, 1);
    do_check_eq(data.uploadTransportFailure, 1);
    do_check_eq(Object.keys(data).length, 3);
  } finally {
    reporter._shutdown();
  }
});

add_task(function test_data_submission_server_failure() {
  let [reporter, server] = yield getReporterAndServer("data_submission_server_failure");
  try {
    Object.defineProperty(server, "_handleNamespaceSubmitPost", {
      value: function (ns, id, request, response) {
        throw HTTP_500;
      },
      writable: true,
    });

    let deferred = Promise.defer();
    let now = new Date();
    let request = new DataSubmissionRequest(deferred, now);
    reporter.requestDataUpload(request);
    yield deferred.promise;
    do_check_eq(request.state, request.SUBMISSION_FAILURE_HARD);

    let data = yield getHealthReportProviderValues(reporter, now);
    do_check_eq(data.firstDocumentUploadAttempt, 1);
    do_check_eq(data.uploadServerFailure, 1);
    do_check_eq(Object.keys(data).length, 3);
  } finally {
    yield shutdownServer(server);
    reporter._shutdown();
  }
});

add_task(function test_data_submission_success() {
  let [reporter, server] = yield getReporterAndServer("data_submission_success");
  try {
    yield reporter._providerManager.registerProviderFromType(DummyProvider);
    yield reporter._providerManager.registerProviderFromType(DummyConstantProvider);

    do_check_eq(reporter.lastPingDate.getTime(), 0);
    do_check_false(reporter.haveRemoteData());

    let deferred = Promise.defer();

    let now = new Date();
    let request = new DataSubmissionRequest(deferred, now);
    reporter._state.addRemoteID("foo");
    reporter.requestDataUpload(request);
    yield deferred.promise;
    do_check_eq(request.state, request.SUBMISSION_SUCCESS);
    do_check_true(reporter.lastPingDate.getTime() > 0);
    do_check_true(reporter.haveRemoteData());
    for (let remoteID of reporter._state.remoteIDs) {
      do_check_neq(remoteID, "foo");
    }

    // Ensure data from providers made it to payload.
    let o = yield reporter.getJSONPayload(true);
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);
    do_check_true("DummyConstantProvider.DummyMeasurement" in o.data.last);

    let data = yield getHealthReportProviderValues(reporter, now);
    do_check_eq(data.continuationUploadAttempt, 1);
    do_check_eq(data.uploadSuccess, 1);
    do_check_eq(Object.keys(data).length, 3);

    let d = reporter.lastPingDate;
    let id = reporter.lastSubmitID;
    let clientID = reporter._state.clientID;

    yield reporter._shutdown();

    // Ensure reloading state works.
    reporter = yield getReporter("data_submission_success");
    do_check_eq(reporter.lastSubmitID, id);
    do_check_eq(reporter.lastPingDate.getTime(), d.getTime());
    do_check_eq(reporter._state.clientID, clientID);

    yield reporter._shutdown();
  } finally {
    yield shutdownServer(server);
  }
});

add_task(function test_recurring_daily_pings() {
  let [reporter, server] = yield getReporterAndServer("recurring_daily_pings");
  try {
    reporter._providerManager.registerProvider(new DummyProvider());

    let policy = reporter._policy;

    defineNow(policy, policy._futureDate(-24 * 60 * 68 * 1000));
    policy.recordUserAcceptance();
    defineNow(policy, policy.nextDataSubmissionDate);
    let promise = policy.checkStateAndTrigger();
    do_check_neq(promise, null);
    yield promise;

    let lastID = reporter.lastSubmitID;
    do_check_neq(lastID, null);
    do_check_true(server.hasDocument(reporter.serverNamespace, lastID));

    // Skip forward to next scheduled submission time.
    defineNow(policy, policy.nextDataSubmissionDate);
    promise = policy.checkStateAndTrigger();
    do_check_neq(promise, null);
    yield promise;
    do_check_neq(reporter.lastSubmitID, lastID);
    do_check_true(server.hasDocument(reporter.serverNamespace, reporter.lastSubmitID));
    do_check_false(server.hasDocument(reporter.serverNamespace, lastID));

    // now() on the health reporter instance wasn't munged. So, we should see
    // both requests attributed to the same day.
    let data = yield getHealthReportProviderValues(reporter, new Date());
    do_check_eq(data.firstDocumentUploadAttempt, 1);
    do_check_eq(data.continuationUploadAttempt, 1);
    do_check_eq(data.uploadSuccess, 2);
    do_check_eq(Object.keys(data).length, 4);
  } finally {
    reporter._shutdown();
    yield shutdownServer(server);
  }
});

add_task(function test_request_remote_data_deletion() {
  let [reporter, server] = yield getReporterAndServer("request_remote_data_deletion");

  try {
    let policy = reporter._policy;
    defineNow(policy, policy._futureDate(-24 * 60 * 60 * 1000));
    policy.recordUserAcceptance();
    defineNow(policy, policy.nextDataSubmissionDate);
    yield policy.checkStateAndTrigger();
    let id = reporter.lastSubmitID;
    do_check_neq(id, null);
    do_check_true(server.hasDocument(reporter.serverNamespace, id));

    let clientID = reporter._state.clientID;
    do_check_neq(clientID, null);

    defineNow(policy, policy._futureDate(10 * 1000));

    let promise = reporter.requestDeleteRemoteData();
    do_check_neq(promise, null);
    yield promise;
    do_check_null(reporter.lastSubmitID);
    do_check_false(reporter.haveRemoteData());
    do_check_false(server.hasDocument(reporter.serverNamespace, id));

    // Client ID should be updated.
    do_check_neq(reporter._state.clientID, null);
    do_check_neq(reporter._state.clientID, clientID);
    do_check_eq(reporter._state.clientIDVersion, 1);

    // And it should be persisted to disk.
    let o = yield CommonUtils.readJSON(reporter._state._filename);
    do_check_eq(o.clientID, reporter._state.clientID);
    do_check_eq(o.clientIDVersion, 1);
  } finally {
    reporter._shutdown();
    yield shutdownServer(server);
  }
});

add_task(function test_multiple_simultaneous_uploads() {
  let [reporter, server] = yield getReporterAndServer("multiple_simultaneous_uploads");

  try {
    let d1 = Promise.defer();
    let d2 = Promise.defer();
    let t1 = new Date(Date.now() - 1000);
    let t2 = new Date(t1.getTime() + 500);
    let r1 = new DataSubmissionRequest(d1, t1);
    let r2 = new DataSubmissionRequest(d2, t2);

    let getPayloadDeferred = Promise.defer();

    Object.defineProperty(reporter, "getJSONPayload", {
      configurable: true,
      value: () => {
        getPayloadDeferred.resolve();
        delete reporter["getJSONPayload"];
        return reporter.getJSONPayload();
      },
    });

    let p1 = reporter.requestDataUpload(r1);
    yield getPayloadDeferred.promise;
    do_check_true(reporter._uploadInProgress);
    let p2 = reporter.requestDataUpload(r2);

    yield p1;
    yield p2;

    do_check_eq(r1.state, r1.SUBMISSION_SUCCESS);
    do_check_eq(r2.state, r2.UPLOAD_IN_PROGRESS);

    // They should both be resolved already.
    yield d1;
    yield d2;

    let data = yield getHealthReportProviderValues(reporter, t1);
    do_check_eq(data.firstDocumentUploadAttempt, 1);
    do_check_false("continuationUploadAttempt" in data);
    do_check_eq(data.uploadSuccess, 1);
    do_check_eq(data.uploadAlreadyInProgress, 1);
  } finally {
    reporter._shutdown();
    yield shutdownServer(server);
  }
});

add_task(function test_policy_accept_reject() {
  let [reporter, server] = yield getReporterAndServer("policy_accept_reject");

  try {
    let policy = reporter._policy;

    do_check_false(policy.dataSubmissionPolicyAccepted);
    do_check_false(reporter.willUploadData);

    policy.recordUserAcceptance();
    do_check_true(policy.dataSubmissionPolicyAccepted);
    do_check_true(reporter.willUploadData);

    policy.recordUserRejection();
    do_check_false(policy.dataSubmissionPolicyAccepted);
    do_check_false(reporter.willUploadData);
  } finally {
    yield reporter._shutdown();
    yield shutdownServer(server);
  }
});

add_task(function test_error_message_scrubbing() {
  let reporter = yield getReporter("error_message_scrubbing");

  try {
    let profile = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    reporter._recordError("Foo " + profile);

    do_check_eq(reporter._errors.length, 1);
    do_check_eq(reporter._errors[0], "Foo <ProfilePath>");

    reporter._errors = [];

    let appdata = Services.dirsvc.get("UAppData", Ci.nsIFile);
    let uri = Services.io.newFileURI(appdata);

    reporter._recordError("Foo " + uri.spec);
    do_check_eq(reporter._errors[0], "Foo <AppDataURI>");
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_basic_appinfo() {
  function verify(d) {
    do_check_eq(d["_v"], 1);
    do_check_eq(d._v, 1);
    do_check_eq(d.vendor, "Mozilla");
    do_check_eq(d.name, "xpcshell");
    do_check_eq(d.id, "xpcshell@tests.mozilla.org");
    do_check_eq(d.version, "1");
    do_check_eq(d.appBuildID, "20121107");
    do_check_eq(d.platformVersion, "p-ver");
    do_check_eq(d.platformBuildID, "20121106");
    do_check_eq(d.os, "XPCShell");
    do_check_eq(d.xpcomabi, "noarch-spidermonkey");
    do_check_true("updateChannel" in d);
  }
  let reporter = yield getReporter("basic_appinfo");
  try {
    verify(reporter.obtainAppInfo());
    let payload = yield reporter.collectAndObtainJSONPayload(true);
    do_check_eq(payload["version"], 2);
    verify(payload["geckoAppInfo"]);
  } finally {
    yield reporter._shutdown();
  }
});

// Ensure collection occurs if upload is disabled.
add_task(function test_collect_when_upload_disabled() {
  let reporter = getHealthReporter("collect_when_upload_disabled");
  reporter._policy.recordHealthReportUploadEnabled(false, "testing-collect");
  do_check_false(reporter._policy.healthReportUploadEnabled);

  let name = "healthreport-testing-collect_when_upload_disabled-healthreport-lastDailyCollection";
  let pref = "app.update.lastUpdateTime." + name;
  do_check_false(Services.prefs.prefHasUserValue(pref));

  try {
    yield reporter.init();
    do_check_true(Services.prefs.prefHasUserValue(pref));

    // We would ideally ensure the timer fires and does the right thing.
    // However, testing the update timer manager is quite involved.
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_failure_if_not_initialized() {
  let reporter = yield getReporter("failure_if_not_initialized");
  yield reporter._shutdown();

  let error = false;
  try {
    yield reporter.requestDataUpload();
  } catch (ex) {
    error = true;
    do_check_true(ex.message.contains("Not initialized."));
  } finally {
    do_check_true(error);
    error = false;
  }

  try {
    yield reporter.collectMeasurements();
  } catch (ex) {
    error = true;
    do_check_true(ex.message.contains("Not initialized."));
  } finally {
    do_check_true(error);
    error = false;
  }

  // getJSONPayload always works (to facilitate error upload).
  yield reporter.getJSONPayload();
});

add_task(function test_upload_on_init_failure() {
  let MESSAGE = "Fake error during provider manager initialization." + Math.random();

  let server = new BagheeraServer();
  server.start();
  let reporter = yield getHealthReporter("upload_on_init_failure", server.serverURI, true);
  server.createNamespace(reporter.serverNamespace);

  reporter.onInitializeProviderManagerFinished = function () {
    throw new Error(MESSAGE);
  };

  let deferred = Promise.defer();

  let oldOnResult = reporter._onBagheeraResult;
  Object.defineProperty(reporter, "_onBagheeraResult", {
    value: function (request, isDelete, date, result) {
      do_check_false(isDelete);
      do_check_true(result.transportSuccess);
      do_check_true(result.serverSuccess);

      oldOnResult.call(reporter, request, isDelete, new Date(), result);
      deferred.resolve();
    },
  });

  reporter._policy.recordUserAcceptance();
  let error = false;
  try {
    yield reporter.init();
  } catch (ex) {
    error = true;
  } finally {
    do_check_true(error);
  }

  // At this point the emergency upload should have been initiated. We
  // wait for our monkeypatched response handler to signal request
  // completion.
  yield deferred.promise;

  do_check_true(server.hasDocument(reporter.serverNamespace, reporter.lastSubmitID));
  let doc = server.getDocument(reporter.serverNamespace, reporter.lastSubmitID);
  do_check_true("notInitialized" in doc);
  do_check_eq(doc.notInitialized, 1);
  do_check_true("errors" in doc);
  do_check_eq(doc.errors.length, 1);
  do_check_true(doc.errors[0].contains(MESSAGE));

  yield reporter._shutdown();
  yield shutdownServer(server);
});

add_task(function test_state_prefs_conversion_simple() {
  let reporter = getHealthReporter("state_prefs_conversion");
  let prefs = reporter._prefs;

  let lastSubmit = new Date();
  prefs.set("lastSubmitID", "lastID");
  CommonUtils.setDatePref(prefs, "lastPingTime", lastSubmit);

  try {
    yield reporter.init();

    do_check_eq(reporter._state.lastSubmitID, "lastID");
    do_check_eq(reporter._state.remoteIDs.length, 1);
    do_check_eq(reporter._state.lastPingDate.getTime(), lastSubmit.getTime());
    do_check_eq(reporter._state.lastPingDate.getTime(), reporter.lastPingDate.getTime());
    do_check_eq(reporter._state.lastSubmitID, reporter.lastSubmitID);
    do_check_true(reporter.haveRemoteData());

    // User set preferences should have been wiped out.
    do_check_false(prefs.isSet("lastSubmitID"));
    do_check_false(prefs.isSet("lastPingTime"));
  } finally {
    yield reporter._shutdown();
  }
});

// If the saved JSON file does not contain an object, we should reset
// automatically.
add_task(function test_state_no_json_object() {
  let reporter = getHealthReporter("state_shared");
  yield CommonUtils.writeJSON("hello", reporter._state._filename);

  try {
    yield reporter.init();

    do_check_eq(reporter.lastPingDate.getTime(), 0);
    do_check_null(reporter.lastSubmitID);

    let o = yield CommonUtils.readJSON(reporter._state._filename);
    do_check_eq(typeof(o), "object");
    do_check_eq(o.v, 1);
    do_check_eq(o.lastPingTime, 0);
    do_check_eq(o.remoteIDs.length, 0);
  } finally {
    yield reporter._shutdown();
  }
});

// If we encounter a future version, we reset state to the current version.
add_task(function test_state_future_version() {
  let reporter = getHealthReporter("state_shared");
  yield CommonUtils.writeJSON({v: 2, remoteIDs: ["foo"], lastPingTime: 2412},
                              reporter._state._filename);
  try {
    yield reporter.init();

    do_check_eq(reporter.lastPingDate.getTime(), 0);
    do_check_null(reporter.lastSubmitID);

    // While the object is updated, we don't save the file.
    let o = yield CommonUtils.readJSON(reporter._state._filename);
    do_check_eq(o.v, 2);
    do_check_eq(o.lastPingTime, 2412);
    do_check_eq(o.remoteIDs.length, 1);
  } finally {
    yield reporter._shutdown();
  }
});

// Test recovery if the state file contains invalid JSON.
add_task(function test_state_invalid_json() {
  let reporter = getHealthReporter("state_shared");

  let encoder = new TextEncoder();
  let arr = encoder.encode("{foo: bad value, 'bad': as2,}");
  let path = reporter._state._filename;
  yield OS.File.writeAtomic(path, arr, {tmpPath: path + ".tmp"});

  try {
    yield reporter.init();

    do_check_eq(reporter.lastPingDate.getTime(), 0);
    do_check_null(reporter.lastSubmitID);
  } finally {
    yield reporter._shutdown();
  }
});

add_task(function test_state_multiple_remote_ids() {
  let [reporter, server] = yield getReporterAndServer("state_multiple_remote_ids");
  let documents = [
    [reporter.serverNamespace, "one", "{v:1}"],
    [reporter.serverNamespace, "two", "{v:2}"],
  ];
  let now = new Date(Date.now() - 5000);

  try {
    for (let [ns, id, payload] of documents) {
      server.setDocument(ns, id, payload);
      do_check_true(server.hasDocument(ns, id));
      yield reporter._state.addRemoteID(id);
      do_check_eq(reporter._state.remoteIDs.indexOf(id), reporter._state.remoteIDs.length - 1);
    }
    yield reporter._state.setLastPingDate(now);
    do_check_eq(reporter._state.remoteIDs.length, 2);
    do_check_eq(reporter.lastSubmitID, documents[0][1]);

    let deferred = Promise.defer();
    let request = new DataSubmissionRequest(deferred, now);
    reporter.requestDataUpload(request);
    yield deferred.promise;

    do_check_eq(reporter._state.remoteIDs.length, 1);
    for (let [,id,] of documents) {
      do_check_eq(reporter._state.remoteIDs.indexOf(id), -1);
      do_check_false(server.hasDocument(reporter.serverNamespace, id));
    }
    do_check_true(reporter.lastPingDate.getTime() > now.getTime());

    let o = yield CommonUtils.readJSON(reporter._state._filename);
    do_check_eq(o.remoteIDs.length, 1);
    do_check_eq(o.remoteIDs[0], reporter._state.remoteIDs[0]);
    do_check_eq(o.lastPingTime, reporter.lastPingDate.getTime());
  } finally {
    yield shutdownServer(server);
    yield reporter._shutdown();
  }
});

// If we have a state file then downgrade to prefs, the prefs should be
// reimported and should supplement existing state.
add_task(function test_state_downgrade_upgrade() {
  let reporter = getHealthReporter("state_shared");

  let now = new Date();

  yield CommonUtils.writeJSON({v: 1, remoteIDs: ["id1", "id2"], lastPingTime: now.getTime()},
                              reporter._state._filename);

  let prefs = reporter._prefs;
  prefs.set("lastSubmitID", "prefID");
  prefs.set("lastPingTime", "" + (now.getTime() + 1000));

  try {
    yield reporter.init();

    do_check_eq(reporter.lastSubmitID, "id1");
    do_check_eq(reporter._state.remoteIDs.length, 3);
    do_check_eq(reporter._state.remoteIDs[2], "prefID");
    do_check_eq(reporter.lastPingDate.getTime(), now.getTime() + 1000);
    do_check_false(prefs.isSet("lastSubmitID"));
    do_check_false(prefs.isSet("lastPingTime"));

    let o = yield CommonUtils.readJSON(reporter._state._filename);
    do_check_eq(o.remoteIDs.length, 3);
    do_check_eq(o.lastPingTime, now.getTime() + 1000);
  } finally {
    yield reporter._shutdown();
  }
});

// Missing client ID in state should be created on state load.
add_task(function* test_state_create_client_id() {
  let reporter = getHealthReporter("state_create_client_id");

  yield CommonUtils.writeJSON({
    v: 1,
    remoteIDs: ["id1", "id2"],
    lastPingTime: Date.now(),
    removeOutdatedLastPayload: true,
  }, reporter._state._filename);

  try {
    yield reporter.init();

    do_check_eq(reporter.lastSubmitID, "id1");
    do_check_neq(reporter._state.clientID, null);
    do_check_eq(reporter._state.clientID.length, 36);
    do_check_eq(reporter._state.clientIDVersion, 1);

    let clientID = reporter._state.clientID;

    // The client ID should be persisted as soon as it is created.
    reporter._shutdown();

    reporter = getHealthReporter("state_create_client_id");
    yield reporter.init();
    do_check_eq(reporter._state.clientID, clientID);
  } finally {
    reporter._shutdown();
  }
});

// Invalid stored client ID is reset automatically.
add_task(function* test_empty_client_id() {
  let reporter = getHealthReporter("state_empty_client_id");

  yield CommonUtils.writeJSON({
    v: 1,
    clientID: "",
    remoteIDs: ["id1", "id2"],
    lastPingTime: Date.now(),
    removeOutdatedLastPayload: true,
  }, reporter._state._filename);

  try {
    yield reporter.init();

    do_check_neq(reporter._state.clientID, null);
    do_check_eq(reporter._state.clientID.length, 36);
  } finally {
    reporter._shutdown();
  }
});

add_task(function* test_nonstring_client_id() {
  let reporter = getHealthReporter("state_nonstring_client_id");

  yield CommonUtils.writeJSON({
    v: 1,
    clientID: 42,
    remoteIDs: ["id1", "id2"],
    lastPingTime: Date.now(),
    remoteOutdatedLastPayload: true,
  }, reporter._state._filename);

  try {
    yield reporter.init();

    do_check_neq(reporter._state.clientID, null);
    do_check_eq(reporter._state.clientID.length, 36);
  } finally {
    reporter._shutdown();
  }
});
