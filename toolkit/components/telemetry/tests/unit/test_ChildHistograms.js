
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
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT", false);
  countHist.add();
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT", true);
  countHist.add();
  countHist.add();
  let categHist = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL");
  categHist.add("Label2");
  categHist.add("Label3");

  let flagKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_FLAG");
  flagKeyed.add("a", 1);
  flagKeyed.add("b", 1);
  let countKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_KEYED_COUNT", false);
  countKeyed.add("a");
  countKeyed.add("b");
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_KEYED_COUNT", true);
  countKeyed.add("a");
  countKeyed.add("b");
  countKeyed.add("b");

  // Test record_in_processes
  let contentLinear = Telemetry.getHistogramById("TELEMETRY_TEST_CONTENT_PROCESS");
  contentLinear.add(10);
  let contentKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_CONTENT_PROCESS");
  contentKeyed.add("content", 1);
  let contentFlag = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG_CONTENT_PROCESS");
  contentFlag.add(true);
  let mainFlag = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG_MAIN_PROCESS");
  mainFlag.add(true);
  let allLinear = Telemetry.getHistogramById("TELEMETRY_TEST_ALL_PROCESSES");
  allLinear.add(10);
  let allChildLinear = Telemetry.getHistogramById("TELEMETRY_TEST_ALL_CHILD_PROCESSES");
  allChildLinear.add(10);
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

add_task(async function() {
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
  await TelemetryController.testSetup();
  if (runningInParent) {
    // Make sure we don't generate unexpected pings due to pref changes.
    await setEmptyPrefWatchlist();
  }

  // Run test in child, don't wait for it to finish.
  run_test_in_child("test_ChildHistograms.js");
  await do_await_remote_message(MESSAGE_CHILD_TEST_DONE);

  await ContentTaskUtils.waitForCondition(() => {
    let payload = TelemetrySession.getPayload("test-ping");
    return payload &&
           "processes" in payload &&
           "content" in payload.processes &&
           "histograms" in payload.processes.content &&
           "TELEMETRY_TEST_COUNT" in payload.processes.content.histograms;
  });

  // Test record_in_processes in main process, too
  let contentLinear = Telemetry.getHistogramById("TELEMETRY_TEST_CONTENT_PROCESS");
  contentLinear.add(20);
  let contentKeyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_CONTENT_PROCESS");
  contentKeyed.add("parent", 1);
  let contentFlag = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG_CONTENT_PROCESS");
  contentFlag.add(true);
  let mainFlag = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG_MAIN_PROCESS");
  mainFlag.add(true);
  let allLinear = Telemetry.getHistogramById("TELEMETRY_TEST_ALL_PROCESSES");
  allLinear.add(20);
  let allChildLinear = Telemetry.getHistogramById("TELEMETRY_TEST_ALL_CHILD_PROCESSES");
  allChildLinear.add(20);

  const payload = TelemetrySession.getPayload("test-ping");
  Assert.ok("processes" in payload, "Should have processes section");
  Assert.ok("content" in payload.processes, "Should have child process section");
  Assert.ok("histograms" in payload.processes.content, "Child process section should have histograms.");
  Assert.ok("keyedHistograms" in payload.processes.content, "Child process section should have keyed histograms.");
  check_histogram_values(payload.processes.content);

  // Check record_in_processes
  // Content Process
  let hs = payload.processes.content.histograms;
  let khs = payload.processes.content.keyedHistograms;
  Assert.ok("TELEMETRY_TEST_CONTENT_PROCESS" in hs, "Should have content process histogram");
  Assert.equal(hs.TELEMETRY_TEST_CONTENT_PROCESS.sum, 10, "Should have correct value");
  Assert.ok("TELEMETRY_TEST_KEYED_CONTENT_PROCESS" in khs, "Should have keyed content process histogram");
  Assert.equal(khs.TELEMETRY_TEST_KEYED_CONTENT_PROCESS.content.sum, 1, "Should have correct value");
  Assert.ok("TELEMETRY_TEST_FLAG_CONTENT_PROCESS" in hs, "Should have content process histogram");
  Assert.equal(hs.TELEMETRY_TEST_FLAG_CONTENT_PROCESS.sum, 1, "Should have correct value");
  Assert.ok("TELEMETRY_TEST_ALL_PROCESSES" in hs, "Should have content process histogram");
  Assert.equal(hs.TELEMETRY_TEST_ALL_PROCESSES.sum, 10, "Should have correct value");
  Assert.ok("TELEMETRY_TEST_ALL_CHILD_PROCESSES" in hs, "Should have content process histogram");
  Assert.equal(hs.TELEMETRY_TEST_ALL_CHILD_PROCESSES.sum, 10, "Should have correct value");
  Assert.ok(!("TELEMETRY_TEST_FLAG_MAIN_PROCESS" in hs), "Should not have main process histogram in child process payload");

  // Main Process
  let mainHs = payload.histograms;
  let mainKhs = payload.keyedHistograms;
  Assert.ok(!("TELEMETRY_TEST_CONTENT_PROCESS" in mainHs), "Should not have content process histogram in main process payload");
  Assert.ok(!("TELEMETRY_TEST_KEYED_CONTENT_PROCESS" in mainKhs), "Should not have keyed content process histogram in main process payload");
  Assert.ok(!("TELEMETRY_TEST_FLAG_CONTENT_PROCESS" in mainHs), "Should not have content process histogram in main process payload");
  Assert.ok("TELEMETRY_TEST_ALL_PROCESSES" in mainHs, "Should have all-process histogram in main process payload");
  Assert.equal(mainHs.TELEMETRY_TEST_ALL_PROCESSES.sum, 20, "Should have correct value");
  Assert.ok(!("TELEMETRY_TEST_ALL_CHILD_PROCESSES" in mainHs), "Should not have all-child process histogram in main process payload");
  Assert.ok("TELEMETRY_TEST_FLAG_MAIN_PROCESS" in mainHs, "Should have main process histogram in main process payload");
  Assert.equal(mainHs.TELEMETRY_TEST_FLAG_MAIN_PROCESS.sum, 1, "Should have correct value");

  do_test_finished();
});
