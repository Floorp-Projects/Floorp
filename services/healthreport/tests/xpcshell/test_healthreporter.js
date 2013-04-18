/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/observers.js");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm");
Cu.import("resource://gre/modules/services/datareporting/policy.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://testing-common/services-common/bagheeraserver.js");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");
Cu.import("resource://testing-common/services/healthreport/utils.jsm");


const SERVER_HOSTNAME = "localhost";
const SERVER_PORT = 8080;
const SERVER_URI = "http://" + SERVER_HOSTNAME + ":" + SERVER_PORT;
const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;


function defineNow(policy, now) {
  print("Adjusting fake system clock to " + now);
  Object.defineProperty(policy, "now", {
    value: function customNow() {
      return now;
    },
    writable: true,
  });
}

function getJustReporter(name, uri=SERVER_URI, inspected=false) {
  let branch = "healthreport.testing." + name + ".";

  let prefs = new Preferences(branch + "healthreport.");
  prefs.set("documentServerURI", uri);
  prefs.set("dbName", name);

  let reporter;

  let policyPrefs = new Preferences(branch + "policy.");
  let policy = new DataReportingPolicy(policyPrefs, prefs, {
    onRequestDataUpload: function (request) {
      reporter.requestDataUpload(request);
    },

    onNotifyDataPolicy: function (request) { },

    onRequestRemoteDelete: function (request) {
      reporter.deleteRemoteData(request);
    },
  });

  let type = inspected ? InspectedHealthReporter : HealthReporter;
  reporter = new type(branch + "healthreport.", policy);

  return reporter;
}

function getReporter(name, uri, inspected) {
  let reporter = getJustReporter(name, uri, inspected);
  return reporter.onInit();
}

function getReporterAndServer(name, namespace="test") {
  return Task.spawn(function get() {
    let reporter = yield getReporter(name, SERVER_URI);
    reporter.serverNamespace = namespace;

    let server = new BagheeraServer(SERVER_URI);
    server.createNamespace(namespace);

    server.start(SERVER_PORT);

    throw new Task.Result([reporter, server]);
  });
}

function shutdownServer(server) {
  let deferred = Promise.defer();
  server.stop(deferred.resolve.bind(deferred));

  return deferred.promise;
}

function run_test() {
  makeFakeAppDir().then(run_next_test, do_throw);
}

add_task(function test_constructor() {
  let reporter = yield getReporter("constructor");

  try {
    do_check_eq(reporter.lastPingDate.getTime(), 0);
    do_check_null(reporter.lastSubmitID);

    reporter.lastSubmitID = "foo";
    do_check_eq(reporter.lastSubmitID, "foo");
    reporter.lastSubmitID = null;
    do_check_null(reporter.lastSubmitID);

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
    reporter._shutdown();
  }
});

add_task(function test_shutdown_normal() {
  let reporter = yield getReporter("shutdown_normal");

  // We can't send "quit-application" notification because the xpcshell runner
  // will shut down!
  reporter._initiateShutdown();
  reporter._waitForShutdown();
});

add_task(function test_shutdown_storage_in_progress() {
  let reporter = yield getJustReporter("shutdown_storage_in_progress", SERVER_URI, true);

  reporter.onStorageCreated = function () {
    print("Faking shutdown during storage initialization.");
    reporter._initiateShutdown();
  };

  reporter._waitForShutdown();
  do_check_eq(reporter.providerManagerShutdownCount, 0);
  do_check_eq(reporter.storageCloseCount, 1);
});

// Ensure that a shutdown triggered while provider manager is initializing
// results in shutdown and storage closure.
add_task(function test_shutdown_provider_manager_in_progress() {
  let reporter = yield getJustReporter("shutdown_provider_manager_in_progress",
                                       SERVER_URI, true);

  reporter.onProviderManagerInitialized = function () {
    print("Faking shutdown during provider manager initialization.");
    reporter._initiateShutdown();
  };

  // This will hang if shutdown logic is busted.
  reporter._waitForShutdown();
  do_check_eq(reporter.providerManagerShutdownCount, 1);
  do_check_eq(reporter.storageCloseCount, 1);
});

// Simulates an error during provider manager initialization and verifies we shut down.
add_task(function test_shutdown_when_provider_manager_errors() {
  let reporter = yield getJustReporter("shutdown_when_provider_manager_errors",
                                       SERVER_URI, true);

  reporter.onInitializeProviderManagerFinished = function () {
    print("Throwing fake error.");
    throw new Error("Fake error during provider manager initialization.");
  };

  // This will hang if shutdown logic is busted.
  reporter._waitForShutdown();
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
    do_check_eq(reporter._providerManager._providers.size, 0);
    yield reporter._providerManager.registerProvidersFromCategoryManager(category);
    do_check_eq(reporter._providerManager._providers.size, 1);
    do_check_true(reporter._storage.hasProvider("DummyProvider"));
    do_check_false(reporter._storage.hasProvider("DummyConstantProvider"));
    do_check_neq(reporter.getProvider("DummyProvider"), null);
    do_check_null(reporter.getProvider("DummyConstantProvider"));

    yield reporter._providerManager.ensurePullOnlyProvidersRegistered();
    do_check_eq(reporter._providerManager._providers.size, 2);
    do_check_true(reporter._storage.hasProvider("DummyConstantProvider"));
    yield reporter.collectMeasurements();
    yield reporter._providerManager.ensurePullOnlyProvidersUnregistered();

    do_check_eq(reporter._providerManager._providers.size, 1);
    do_check_true(reporter._storage.hasProvider("DummyConstantProvider"));

    let mID = reporter._storage.measurementID("DummyConstantProvider", "DummyMeasurement", 1);
    let values = yield reporter._storage.getMeasurementValues(mID);
    do_check_true(values.singular.size > 0);
  } finally {
    reporter._shutdown();
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
    reporter._shutdown();
  }
});

add_task(function test_json_payload_simple() {
  let reporter = yield getReporter("json_payload_simple");

  try {
    let now = new Date();
    let payload = yield reporter.getJSONPayload();
    do_check_eq(typeof payload, "string");
    let original = JSON.parse(payload);

    do_check_eq(original.version, 2);
    do_check_eq(original.thisPingDate, reporter._formatDate(now));
    do_check_eq(Object.keys(original.data.last).length, 0);
    do_check_eq(Object.keys(original.data.days).length, 0);
    do_check_false("notInitialized" in original);

    reporter.lastPingDate = new Date(now.getTime() - 24 * 60 * 60 * 1000 - 10);

    original = JSON.parse(yield reporter.getJSONPayload());
    do_check_eq(original.lastPingDate, reporter._formatDate(reporter.lastPingDate));

    // This could fail if we cross UTC day boundaries at the exact instance the
    // test is executed. Let's tempt fate.
    do_check_eq(original.thisPingDate, reporter._formatDate(now));

    payload = yield reporter.getJSONPayload(true);
    do_check_eq(typeof payload, "object");
  } finally {
    reporter._shutdown();
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
    reporter._shutdown();
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
    reporter._shutdown();
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
    yield reporter._providerManager.registerProvidersFromCategoryManager(category);

    let payload = yield reporter.collectAndObtainJSONPayload();
    let o = JSON.parse(payload);
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);
    do_check_true("DummyConstantProvider.DummyMeasurement" in o.data.last);

    let providers = reporter._providerManager.providers;
    do_check_eq(providers.length, 1);

    // Do it again for good measure.
    payload = yield reporter.collectAndObtainJSONPayload();
    o = JSON.parse(payload);
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);
    do_check_true("DummyConstantProvider.DummyMeasurement" in o.data.last);

    providers = reporter._providerManager.providers;
    do_check_eq(providers.length, 1);

    // Ensure throwing getJSONPayload is handled properly.
    Object.defineProperty(reporter, "_getJSONPayload", {
      value: function () {
        throw new Error("Silly error.");
      },
    });

    let deferred = Promise.defer();

    reporter.collectAndObtainJSONPayload().then(do_throw, function onError() {
      providers = reporter._providerManager.providers;
      do_check_eq(providers.length, 1);
      deferred.resolve();
    });

    yield deferred.promise;
  } finally {
    reporter._shutdown();
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
      yield m.addDailyDiscreteNumeric("daily-discrete-numeric", i, date);
      yield m.addDailyDiscreteNumeric("daily-discrete-numeric", i + 100, date);
      yield m.addDailyDiscreteText("daily-discrete-text", "" + i, date);
      yield m.addDailyDiscreteText("daily-discrete-text", "" + (i + 50), date);
      yield m.setDailyLastNumeric("daily-last-numeric", date.getTime(), date);
    }

    let payload = yield reporter.getJSONPayload();
    print(payload);
    let o = JSON.parse(payload);

    do_check_eq(Object.keys(o.data.days).length, 180);
    let today = reporter._formatDate(now);
    do_check_true(today in o.data.days);
  } finally {
    reporter._shutdown();
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
    reporter._shutdown();
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
    reporter._shutdown();
  }
});

add_task(function test_data_submission_transport_failure() {
  let reporter = yield getReporter("data_submission_transport_failure");
  try {
    reporter.serverURI = "http://localhost:8080/";
    reporter.serverNamespace = "test00";

    let deferred = Promise.defer();
    let request = new DataSubmissionRequest(deferred, new Date(Date.now + 30000));
    reporter.requestDataUpload(request);

    yield deferred.promise;
    do_check_eq(request.state, request.SUBMISSION_FAILURE_SOFT);
  } finally {
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

    let request = new DataSubmissionRequest(deferred, new Date());
    reporter.requestDataUpload(request);
    yield deferred.promise;
    do_check_eq(request.state, request.SUBMISSION_SUCCESS);
    do_check_true(reporter.lastPingDate.getTime() > 0);
    do_check_true(reporter.haveRemoteData());

    // Ensure data from providers made it to payload.
    let o = yield reporter.getLastPayload();
    do_check_true("DummyProvider.DummyMeasurement" in o.data.last);
    do_check_true("DummyConstantProvider.DummyMeasurement" in o.data.last);

    reporter._shutdown();
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

    defineNow(policy, policy._futureDate(10 * 1000));

    let promise = reporter.requestDeleteRemoteData();
    do_check_neq(promise, null);
    yield promise;
    do_check_null(reporter.lastSubmitID);
    do_check_false(reporter.haveRemoteData());
    do_check_false(server.hasDocument(reporter.serverNamespace, id));
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
    reporter._shutdown();
    yield shutdownServer(server);
  }
});

add_task(function test_upload_save_payload() {
  let [reporter, server] = yield getReporterAndServer("upload_save_payload");

  try {
    let deferred = Promise.defer();
    let request = new DataSubmissionRequest(deferred, new Date(), false);

    yield reporter._uploadData(request);
    let json = yield reporter.getLastPayload();
    do_check_true("thisPingDate" in json);
  } finally {
    reporter._shutdown();
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
    reporter._shutdown();
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
    reporter._shutdown();
  }
});

// Ensure collection occurs if upload is disabled.
add_task(function test_collect_when_upload_disabled() {
  let reporter = getJustReporter("collect_when_upload_disabled");
  reporter._policy.recordHealthReportUploadEnabled(false, "testing-collect");
  do_check_false(reporter._policy.healthReportUploadEnabled);

  let name = "healthreport-testing-collect_when_upload_disabled-healthreport-lastDailyCollection";
  let pref = "app.update.lastUpdateTime." + name;
  do_check_false(Services.prefs.prefHasUserValue(pref));
  try {
    yield reporter.onInit();
    do_check_true(Services.prefs.prefHasUserValue(pref));

    // We would ideally ensure the timer fires and does the right thing.
    // However, testing the update timer manager is quite involved.
  } finally {
    reporter._shutdown();
  }
});

add_task(function test_failure_if_not_initialized() {
  let reporter = yield getReporter("failure_if_not_initialized");
  reporter._shutdown();

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
  let reporter = yield getJustReporter("upload_on_init_failure", SERVER_URI, true);
  let server = new BagheeraServer(SERVER_URI);
  server.createNamespace(reporter.serverNamespace);
  server.start(SERVER_PORT);

  reporter.onInitializeProviderManagerFinished = function () {
    throw new Error("Fake error during provider manager initialization.");
  };

  let deferred = Promise.defer();

  let oldOnResult = reporter._onBagheeraResult;
  Object.defineProperty(reporter, "_onBagheeraResult", {
    value: function (request, isDelete, result) {
      do_check_false(isDelete);
      do_check_true(result.transportSuccess);
      do_check_true(result.serverSuccess);

      oldOnResult.call(reporter, request, isDelete, result);
      deferred.resolve();
    },
  });

  reporter._policy.recordUserAcceptance();
  let error = false;
  try {
    yield reporter.onInit();
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
  do_check_true(doc.errors[0].contains("Fake error during provider manager initialization"));

  reporter._shutdown();
  yield shutdownServer(server);
});

