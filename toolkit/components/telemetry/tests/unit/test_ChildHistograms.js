
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);

const MESSAGE_TELEMETRY_PAYLOAD = "Telemetry:Payload";

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

  let flagKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_FLAG");
  flagKeyed.add("a", 1);
  flagKeyed.add("b", 1);
  let countKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  countKeyed.add("a");
  countKeyed.add("b");
  countKeyed.add("b");

  // Check payload values.
  const payload = TelemetrySession.getPayload("test-ping");
  check_histogram_values(payload);
}

function check_histogram_values(payload) {
  const hs = payload.histograms;
  Assert.ok("TELEMETRY_TEST_COUNT" in hs, "Should have count test histogram.");
  Assert.ok("TELEMETRY_TEST_FLAG" in hs, "Should have flag test histogram.");
  Assert.equal(hs["TELEMETRY_TEST_COUNT"].sum, 2,
               "Count test histogram should have the right value.");
  Assert.equal(hs["TELEMETRY_TEST_FLAG"].sum, 1,
               "Flag test histogram should have the right value.");

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
    TelemetryController.setupContent();
    run_child_test();
    return;
  }

  // Setup.
  do_get_profile(true);
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  Services.prefs.setBoolPref("toolkit.telemetry.enabled", true);
  yield TelemetryController.setup();
  yield TelemetrySession.setup();

  // Run test in child and wait until it is finished.
  yield run_test_in_child("test_ChildHistograms.js");

  // Gather payload from child.
  let promiseMessage = do_await_remote_message(MESSAGE_TELEMETRY_PAYLOAD);
  TelemetrySession.requestChildPayloads();
  yield promiseMessage;

  // Check child payload.
  const payload = TelemetrySession.getPayload("test-ping");
  Assert.ok("childPayloads" in payload, "Should have child payloads.");
  Assert.equal(payload.childPayloads.length, 1, "Should have received one child payload so far.");
  Assert.ok("histograms" in payload.childPayloads[0], "Child payload should have histograms.");
  Assert.ok("keyedHistograms" in payload.childPayloads[0], "Child payload should have keyed histograms.");
  check_histogram_values(payload.childPayloads[0]);

  do_test_finished();
});
