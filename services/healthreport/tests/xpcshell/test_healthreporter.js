/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm");
Cu.import("resource://gre/modules/services/healthreport/policy.jsm");
Cu.import("resource://testing-common/services-common/bagheeraserver.js");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");


const SERVER_HOSTNAME = "localhost";
const SERVER_PORT = 8080;
const SERVER_URI = "http://" + SERVER_HOSTNAME + ":" + SERVER_PORT;


function defineNow(policy, now) {
  print("Adjusting fake system clock to " + now);
  Object.defineProperty(policy, "now", {
    value: function customNow() {
      return now;
    },
    writable: true,
  });
}

function getReporter(name, uri=SERVER_URI) {
  let branch = "healthreport.testing. " + name + ".";

  let prefs = new Preferences(branch);
  prefs.set("documentServerURI", uri);

  return new HealthReporter(branch);
}

function getReporterAndServer(name, namespace="test") {
  let reporter = getReporter(name, SERVER_URI);
  reporter.serverNamespace = namespace;

  let server = new BagheeraServer(SERVER_URI);
  server.createNamespace(namespace);

  server.start(SERVER_PORT);

  return [reporter, server];
}

function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let reporter = getReporter("constructor");

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
    do_check_true(ex.message.startsWith("Branch argument must end"));
  } finally {
    do_check_true(failed);
    failed = false;
  }

  run_next_test();
});

add_test(function test_register_providers_from_category_manager() {
  const category = "healthreporter-js-modules";

  let cm = Cc["@mozilla.org/categorymanager;1"]
             .getService(Ci.nsICategoryManager);
  cm.addCategoryEntry(category, "DummyProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);

  let reporter = getReporter("category_manager");
  do_check_eq(reporter._collector._providers.length, 0);
  reporter.registerProvidersFromCategoryManager(category);
  do_check_eq(reporter._collector._providers.length, 1);

  run_next_test();
});

add_test(function test_start() {
  let reporter = getReporter("start");
  reporter.start().then(function onStarted() {
    reporter.stop();
    run_next_test();
  });
});

add_test(function test_json_payload_simple() {
  let reporter = getReporter("json_payload_simple");

  let now = new Date();
  let payload = reporter.getJSONPayload();
  let original = JSON.parse(payload);

  do_check_eq(original.version, 1);
  do_check_eq(original.thisPingDate, reporter._formatDate(now));
  do_check_eq(Object.keys(original.providers).length, 0);

  reporter.lastPingDate = new Date(now.getTime() - 24 * 60 * 60 * 1000 - 10);

  original = JSON.parse(reporter.getJSONPayload());
  do_check_eq(original.lastPingDate, reporter._formatDate(reporter.lastPingDate));

  // This could fail if we cross UTC day boundaries at the exact instance the
  // test is executed. Let's tempt fate.
  do_check_eq(original.thisPingDate, reporter._formatDate(now));

  run_next_test();
});

add_test(function test_json_payload_dummy_provider() {
  let reporter = getReporter("json_payload_dummy_provider");

  reporter.registerProvider(new DummyProvider());
  reporter.collectMeasurements().then(function onResult() {
    let o = JSON.parse(reporter.getJSONPayload());

    do_check_eq(Object.keys(o.providers).length, 1);
    do_check_true("DummyProvider" in o.providers);
    do_check_true("measurements" in o.providers.DummyProvider);
    do_check_true("DummyMeasurement" in o.providers.DummyProvider.measurements);

    run_next_test();
  });
});

add_test(function test_notify_policy_observers() {
  let reporter = getReporter("notify_policy_observers");

  Observers.add("healthreport:notify-data-policy:request",
                function onObserver(subject, data) {
    Observers.remove("healthreport:notify-data-policy:request", onObserver);

    do_check_true("foo" in subject);

    run_next_test();
  });

  reporter.onNotifyDataPolicy({foo: "bar"});
});

add_test(function test_data_submission_transport_failure() {
  let reporter = getReporter("data_submission_transport_failure");
  reporter.serverURI = "http://localhost:8080/";
  reporter.serverNamespace = "test00";

  let deferred = Promise.defer();
  deferred.promise.then(function onResult(request) {
    do_check_eq(request.state, request.SUBMISSION_FAILURE_SOFT);

    run_next_test();
  });

  let request = new DataSubmissionRequest(deferred, new Date(Date.now + 30000));
  reporter.onRequestDataUpload(request);
});

add_test(function test_data_submission_success() {
  let [reporter, server] = getReporterAndServer("data_submission_success");

  do_check_eq(reporter.lastPingDate.getTime(), 0);
  do_check_false(reporter.haveRemoteData());

  let deferred = Promise.defer();
  deferred.promise.then(function onResult(request) {
    do_check_eq(request.state, request.SUBMISSION_SUCCESS);
    do_check_neq(reporter.lastPingDate.getTime(), 0);
    do_check_true(reporter.haveRemoteData());

    server.stop(run_next_test);
  });

  let request = new DataSubmissionRequest(deferred, new Date());
  reporter.onRequestDataUpload(request);
});

add_test(function test_recurring_daily_pings() {
  let [reporter, server] = getReporterAndServer("recurring_daily_pings");
  reporter.registerProvider(new DummyProvider());

  let policy = reporter._policy;

  defineNow(policy, policy._futureDate(-24 * 60 * 68 * 1000));
  policy.recordUserAcceptance();
  defineNow(policy, policy.nextDataSubmissionDate);
  let promise = policy.checkStateAndTrigger();
  do_check_neq(promise, null);

  promise.then(function onUploadComplete() {
    let lastID = reporter.lastSubmitID;

    do_check_neq(lastID, null);
    do_check_true(server.hasDocument(reporter.serverNamespace, lastID));

    // Skip forward to next scheduled submission time.
    defineNow(policy, policy.nextDataSubmissionDate);
    let promise = policy.checkStateAndTrigger();
    do_check_neq(promise, null);
    promise.then(function onSecondUploadCOmplete() {
      do_check_neq(reporter.lastSubmitID, lastID);
      do_check_true(server.hasDocument(reporter.serverNamespace, reporter.lastSubmitID));
      do_check_false(server.hasDocument(reporter.serverNamespace, lastID));

      server.stop(run_next_test);
    });
  });
});

add_test(function test_request_remote_data_deletion() {
  let [reporter, server] = getReporterAndServer("request_remote_data_deletion");

  let policy = reporter._policy;
  defineNow(policy, policy._futureDate(-24 * 60 * 60 * 1000));
  policy.recordUserAcceptance();
  defineNow(policy, policy.nextDataSubmissionDate);
  policy.checkStateAndTrigger().then(function onUploadComplete() {
    let id = reporter.lastSubmitID;
    do_check_neq(id, null);
    do_check_true(server.hasDocument(reporter.serverNamespace, id));

    defineNow(policy, policy._futureDate(10 * 1000));

    let promise = reporter.requestDeleteRemoteData();
    do_check_neq(promise, null);
    promise.then(function onDeleteComplete() {
      do_check_null(reporter.lastSubmitID);
      do_check_false(reporter.haveRemoteData());
      do_check_false(server.hasDocument(reporter.serverNamespace, id));

      server.stop(run_next_test);
    });
  });
});

add_test(function test_policy_accept_reject() {
  let [reporter, server] = getReporterAndServer("policy_accept_reject");

  do_check_false(reporter.dataSubmissionPolicyAccepted);
  do_check_false(reporter.willUploadData);

  reporter.recordPolicyAcceptance();
  do_check_true(reporter.dataSubmissionPolicyAccepted);
  do_check_true(reporter.willUploadData);

  reporter.recordPolicyRejection();
  do_check_false(reporter.dataSubmissionPolicyAccepted);
  do_check_false(reporter.willUploadData);

  server.stop(run_next_test);
});


add_test(function test_upload_save_payload() {
  let [reporter, server] = getReporterAndServer("upload_save_payload");

  let deferred = Promise.defer();
  let request = new DataSubmissionRequest(deferred, new Date(), false);

  reporter._uploadData(request).then(function onUpload() {
    reporter.getLastPayload().then(function onJSON(json) {
      do_check_true("thisPingDate" in json);
      server.stop(run_next_test);
    });
  });
});

