
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

const MESSAGE_TELEMETRY_PAYLOAD = "Telemetry:Payload";
const MESSAGE_TELEMETRY_GET_CHILD_PAYLOAD = "Telemetry:GetChildPayload";
const MESSAGE_CHILD_TEST_DONE = "ChildTest:Done";

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

function run_child_test() {
  // Setup histograms with some fixed values.
  let flagHist = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  flagHist.add(1);
  let countHist = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  countHist.add();
  countHist.add();
  let categHist = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL");
  categHist.add("Label2");
  categHist.add("Label3");

  let flagKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_FLAG");
  flagKeyed.add("a", 1);
  flagKeyed.add("b", 1);
  let countKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  countKeyed.add("a");
  countKeyed.add("b");
  countKeyed.add("b");
}

function check_histogram_values(payload) {
  const hs = payload.histograms;
  Assert.ok("TELEMETRY_TEST_COUNT" in hs, "Should have count test histogram.");
  Assert.ok("TELEMETRY_TEST_FLAG" in hs, "Should have flag test histogram.");
  Assert.ok("TELEMETRY_TEST_CATEGORICAL" in hs, "Should have categorical test histogram.");
  Assert.equal(hs["TELEMETRY_TEST_COUNT"].sum, 2,
               "Count test histogram should have the right value.");
  Assert.equal(hs["TELEMETRY_TEST_FLAG"].sum, 1,
               "Flag test histogram should have the right value.");
  Assert.equal(hs["TELEMETRY_TEST_CATEGORICAL"].sum, 3,
               "Categorical test histogram should have the right sum.");

  const kh = payload.keyedHistograms;
  Assert.ok("TELEMETRY_TEST_KEYED_COUNT" in kh, "Should have keyed count test histogram.");
  Assert.ok("TELEMETRY_TEST_KEYED_FLAG" in kh, "Should have keyed flag test histogram.");
  Assert.equal(kh["TELEMETRY_TEST_KEYED_COUNT"]["a"].sum, 1,
               "Keyed count test histogram should have the right value.");
  Assert.equal(kh["TELEMETRY_TEST_KEYED_COUNT"]["b"].sum, 2,
               "Keyed count test histogram should have the right value.");
  Assert.equal(kh["TELEMETRY_TEST_KEYED_FLAG"]["a"].sum, 1,
               "Keyed flag test histogram should have the right value.");
  Assert.equal(kh["TELEMETRY_TEST_KEYED_FLAG"]["b"].sum, 1,
               "Keyed flag test histogram should have the right value.");
}

add_task(function*() {
  if (!runningInParent) {
    TelemetryController.testSetupContent();
    run_child_test();
    dump("... done with child test\n");
    do_send_remote_message(MESSAGE_CHILD_TEST_DONE);
    return;
  }

  // Setup.
  do_get_profile(true);
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  yield TelemetryController.testSetup();
  if (runningInParent) {
    // Make sure we don't generate unexpected pings due to pref changes.
    yield setEmptyPrefWatchlist();
  }

  // Run test in child, don't wait for it to finish.
  let childPromise = run_test_in_child("test_ChildHistograms.js");
  yield do_await_remote_message(MESSAGE_CHILD_TEST_DONE);

  yield ContentTaskUtils.waitForCondition(() => {
    let payload = TelemetrySession.getPayload("test-ping");
    return payload &&
           "processes" in payload &&
           "content" in payload.processes &&
           "histograms" in payload.processes.content &&
           "TELEMETRY_TEST_COUNT" in payload.processes.content.histograms;
  });
  const payload = TelemetrySession.getPayload("test-ping");
  Assert.ok("processes" in payload, "Should have processes section");
  Assert.ok("content" in payload.processes, "Should have child process section");
  Assert.ok("histograms" in payload.processes.content, "Child process section should have histograms.");
  Assert.ok("keyedHistograms" in payload.processes.content, "Child process section should have keyed histograms.");
  check_histogram_values(payload.processes.content);

  do_test_finished();
});
