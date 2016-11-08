/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INT_MAX = 0x7FFFFFFF;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);

// Return an array of numbers from lower up to, excluding, upper
function numberRange(lower, upper)
{
  let a = [];
  for (let i=lower; i<upper; ++i) {
    a.push(i);
  }
  return a;
}

function expect_fail(f) {
  let failed = false;
  try {
    f();
    failed = false;
  } catch (e) {
    failed = true;
  }
  do_check_true(failed);
}

function expect_success(f) {
  let succeeded = false;
  try {
    f();
    succeeded = true;
  } catch (e) {
    succeeded = false;
  }
  do_check_true(succeeded);
}

function compareHistograms(h1, h2) {
  let s1 = h1.snapshot();
  let s2 = h2.snapshot();

  do_check_eq(s1.histogram_type, s2.histogram_type);
  do_check_eq(s1.min, s2.min);
  do_check_eq(s1.max, s2.max);
  do_check_eq(s1.sum, s2.sum);

  do_check_eq(s1.counts.length, s2.counts.length);
  for (let i = 0; i < s1.counts.length; i++)
    do_check_eq(s1.counts[i], s2.counts[i]);

  do_check_eq(s1.ranges.length, s2.ranges.length);
  for (let i = 0; i < s1.ranges.length; i++)
    do_check_eq(s1.ranges[i], s2.ranges[i]);
}

function check_histogram(histogram_type, name, min, max, bucket_count) {
  var h = Telemetry.getHistogramById(name);
  var r = h.snapshot().ranges;
  var sum = 0;
  for (let i=0;i<r.length;i++) {
    var v = r[i];
    sum += v;
    h.add(v);
  }
  var s = h.snapshot();
  // verify properties
  do_check_eq(sum, s.sum);

  // there should be exactly one element per bucket
  for (let i of s.counts) {
    do_check_eq(i, 1);
  }
  var hgrams = Telemetry.histogramSnapshots
  let gh = hgrams[name]
  do_check_eq(gh.histogram_type, histogram_type);

  do_check_eq(gh.min, min)
  do_check_eq(gh.max, max)

  // Check that booleans work with nonboolean histograms
  h.add(false);
  h.add(true);
  s = h.snapshot().counts;
  do_check_eq(s[0], 2)
  do_check_eq(s[1], 2)

  // Check that clearing works.
  h.clear();
  s = h.snapshot();
  for (var i of s.counts) {
    do_check_eq(i, 0);
  }
  do_check_eq(s.sum, 0);

  h.add(0);
  h.add(1);
  var c = h.snapshot().counts;
  do_check_eq(c[0], 1);
  do_check_eq(c[1], 1);
}

// This MUST be the very first test of this file.
add_task({
  skip_if: () => gIsAndroid
},
function* test_instantiate() {
  const ID = "TELEMETRY_TEST_COUNT";
  let h = Telemetry.getHistogramById(ID);

  // Instantiate the subsession histogram through |add| and make sure they match.
  // This MUST be the first use of "TELEMETRY_TEST_COUNT" in this file, otherwise
  // |add| will not instantiate the histogram.
  h.add(1);
  let snapshot = h.snapshot();
  let subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.equal(snapshot.sum, subsession[ID].sum,
               "Histogram and subsession histogram sum must match.");
  // Clear the histogram, so we don't void the assumptions from the other tests.
  h.clear();
});

add_task(function* test_parameterChecks() {
  let kinds = [Telemetry.HISTOGRAM_EXPONENTIAL, Telemetry.HISTOGRAM_LINEAR]
  let testNames = ["TELEMETRY_TEST_EXPONENTIAL", "TELEMETRY_TEST_LINEAR"]
  for (let i = 0; i < kinds.length; i++) {
    let histogram_type = kinds[i];
    let test_type = testNames[i];
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10]
    check_histogram(histogram_type, test_type, min, max, bucket_count);
  }
});

add_task(function* test_noSerialization() {
  // Instantiate the storage for this histogram and make sure it doesn't
  // get reflected into JS, as it has no interesting data in it.
  Telemetry.getHistogramById("NEWTAB_PAGE_PINNED_SITES_COUNT");
  do_check_false("NEWTAB_PAGE_PINNED_SITES_COUNT" in Telemetry.histogramSnapshots);
});

add_task(function* test_boolean_histogram() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var r = h.snapshot().ranges;
  // boolean histograms ignore numeric parameters
  do_check_eq(uneval(r), uneval([0, 1, 2]))
  var sum = 0
  for (var i=0;i<r.length;i++) {
    var v = r[i];
    sum += v;
    h.add(v);
  }
  h.add(true);
  h.add(false);
  var s = h.snapshot();
  do_check_eq(s.histogram_type, Telemetry.HISTOGRAM_BOOLEAN);
  // last bucket should always be 0 since .add parameters are normalized to either 0 or 1
  do_check_eq(s.counts[2], 0);
  do_check_eq(s.sum, 3);
  do_check_eq(s.counts[0], 2);
});

add_task(function* test_flag_histogram() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  var r = h.snapshot().ranges;
  // Flag histograms ignore numeric parameters.
  do_check_eq(uneval(r), uneval([0, 1, 2]));
  // Should already have a 0 counted.
  var c = h.snapshot().counts;
  var s = h.snapshot().sum;
  do_check_eq(uneval(c), uneval([1, 0, 0]));
  do_check_eq(s, 0);
  // Should switch counts.
  h.add(1);
  var c2 = h.snapshot().counts;
  var s2 = h.snapshot().sum;
  do_check_eq(uneval(c2), uneval([0, 1, 0]));
  do_check_eq(s2, 1);
  // Should only switch counts once.
  h.add(1);
  var c3 = h.snapshot().counts;
  var s3 = h.snapshot().sum;
  do_check_eq(uneval(c3), uneval([0, 1, 0]));
  do_check_eq(s3, 1);
  do_check_eq(h.snapshot().histogram_type, Telemetry.HISTOGRAM_FLAG);
});

add_task(function* test_count_histogram() {
  let h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT2");
  let s = h.snapshot();
  do_check_eq(uneval(s.ranges), uneval([0, 1, 2]));
  do_check_eq(uneval(s.counts), uneval([0, 0, 0]));
  do_check_eq(s.sum, 0);
  h.add();
  s = h.snapshot();
  do_check_eq(uneval(s.counts), uneval([1, 0, 0]));
  do_check_eq(s.sum, 1);
  h.add();
  s = h.snapshot();
  do_check_eq(uneval(s.counts), uneval([2, 0, 0]));
  do_check_eq(s.sum, 2);
});

add_task(function* test_categorical_histogram()
{
  let h1 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL");
  for (let v of ["CommonLabel", "Label2", "Label3", "Label3", 0, 0, 1]) {
    h1.add(v);
  }
  for (let s of ["", "Label4", "1234"]) {
    Assert.throws(() => h1.add(s));
  }

  let snapshot = h1.snapshot();
  Assert.equal(snapshot.sum, 6);
  Assert.deepEqual(snapshot.ranges, [0, 1, 2, 3]);
  Assert.deepEqual(snapshot.counts, [3, 2, 2, 0]);

  let h2 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL_OPTOUT");
  for (let v of ["CommonLabel", "CommonLabel", "Label4", "Label5", "Label6", 0, 1]) {
    h2.add(v);
  }
  for (let s of ["", "Label3", "1234"]) {
    Assert.throws(() => h2.add(s));
  }

  snapshot = h2.snapshot();
  Assert.equal(snapshot.sum, 7);
  Assert.deepEqual(snapshot.ranges, [0, 1, 2, 3, 4]);
  Assert.deepEqual(snapshot.counts, [3, 2, 1, 1, 0]);
});

add_task(function* test_getHistogramById() {
  try {
    Telemetry.getHistogramById("nonexistent");
    do_throw("This can't happen");
  } catch (e) {

  }
  var h = Telemetry.getHistogramById("CYCLE_COLLECTOR");
  var s = h.snapshot();
  do_check_eq(s.histogram_type, Telemetry.HISTOGRAM_EXPONENTIAL);
  do_check_eq(s.min, 1);
  do_check_eq(s.max, 10000);
});

add_task(function* test_getSlowSQL() {
  var slow = Telemetry.slowSQL;
  do_check_true(("mainThread" in slow) && ("otherThreads" in slow));
});

add_task(function* test_getWebrtc() {
  var webrtc = Telemetry.webrtcStats;
  do_check_true("IceCandidatesStats" in webrtc);
  var icestats = webrtc.IceCandidatesStats;
  do_check_true("webrtc" in icestats);
});

// Check that telemetry doesn't record in private mode
add_task(function* test_privateMode() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var orig = h.snapshot();
  Telemetry.canRecordExtended = false;
  h.add(1);
  do_check_eq(uneval(orig), uneval(h.snapshot()));
  Telemetry.canRecordExtended = true;
  h.add(1);
  do_check_neq(uneval(orig), uneval(h.snapshot()));
});

// Check that telemetry records only when it is suppose to.
add_task(function* test_histogramRecording() {
  // Check that no histogram is recorded if both base and extended recording are off.
  Telemetry.canRecordBase = false;
  Telemetry.canRecordExtended = false;

  let h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTOUT");
  h.clear();
  let orig = h.snapshot();
  h.add(1);
  Assert.equal(orig.sum, h.snapshot().sum);

  // Check that only base histograms are recorded.
  Telemetry.canRecordBase = true;
  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
               "Histogram value should have incremented by 1 due to recording.");

  // Extended histograms should not be recorded.
  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTIN");
  orig = h.snapshot();
  h.add(1);
  Assert.equal(orig.sum, h.snapshot().sum,
               "Histograms should be equal after recording.");

  // Runtime created histograms should not be recorded.
  h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  orig = h.snapshot();
  h.add(1);
  Assert.equal(orig.sum, h.snapshot().sum,
               "Histograms should be equal after recording.");

  // Check that extended histograms are recorded when required.
  Telemetry.canRecordExtended = true;

  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
               "Runtime histogram value should have incremented by 1 due to recording.");

  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTIN");
  orig = h.snapshot();
  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
               "Histogram value should have incremented by 1 due to recording.");

  // Check that base histograms are still being recorded.
  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTOUT");
  h.clear();
  orig = h.snapshot();
  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
               "Histogram value should have incremented by 1 due to recording.");
});

add_task(function* test_addons() {
  var addon_id = "testing-addon";
  var fake_addon_id = "fake-addon";
  var name1 = "testing-histogram1";
  var register = Telemetry.registerAddonHistogram;
  expect_success(() =>
                 register(addon_id, name1, Telemetry.HISTOGRAM_LINEAR, 1, 5, 6));
  // Can't register the same histogram multiple times.
  expect_fail(() =>
              register(addon_id, name1, Telemetry.HISTOGRAM_LINEAR, 1, 5, 6));
  // Make sure we can't get at it with another name.
  expect_fail(() => Telemetry.getAddonHistogram(fake_addon_id, name1));

  // Check for reflection capabilities.
  var h1 = Telemetry.getAddonHistogram(addon_id, name1);
  // Verify that although we've created storage for it, we don't reflect it into JS.
  var snapshots = Telemetry.addonHistogramSnapshots;
  do_check_false(name1 in snapshots[addon_id]);
  h1.add(1);
  h1.add(3);
  var s1 = h1.snapshot();
  do_check_eq(s1.histogram_type, Telemetry.HISTOGRAM_LINEAR);
  do_check_eq(s1.min, 1);
  do_check_eq(s1.max, 5);
  do_check_eq(s1.counts[1], 1);
  do_check_eq(s1.counts[3], 1);

  var name2 = "testing-histogram2";
  expect_success(() =>
                 register(addon_id, name2, Telemetry.HISTOGRAM_LINEAR, 2, 4, 4));

  var h2 = Telemetry.getAddonHistogram(addon_id, name2);
  h2.add(2);
  h2.add(3);
  var s2 = h2.snapshot();
  do_check_eq(s2.histogram_type, Telemetry.HISTOGRAM_LINEAR);
  do_check_eq(s2.min, 2);
  do_check_eq(s2.max, 4);
  do_check_eq(s2.counts[1], 1);
  do_check_eq(s2.counts[2], 1);

  // Check that we can register histograms for a different addon with
  // identical names.
  var extra_addon = "testing-extra-addon";
  expect_success(() =>
                 register(extra_addon, name1, Telemetry.HISTOGRAM_BOOLEAN));

  // Check that we can register flag histograms.
  var flag_addon = "testing-flag-addon";
  var flag_histogram = "flag-histogram";
  expect_success(() =>
                 register(flag_addon, flag_histogram, Telemetry.HISTOGRAM_FLAG));
  expect_success(() =>
                 register(flag_addon, name2, Telemetry.HISTOGRAM_LINEAR, 2, 4, 4));

  // Check that we reflect registered addons and histograms.
  snapshots = Telemetry.addonHistogramSnapshots;
  do_check_true(addon_id in snapshots)
  do_check_true(extra_addon in snapshots);
  do_check_true(flag_addon in snapshots);

  // Check that we have data for our created histograms.
  do_check_true(name1 in snapshots[addon_id]);
  do_check_true(name2 in snapshots[addon_id]);
  var s1_alt = snapshots[addon_id][name1];
  var s2_alt = snapshots[addon_id][name2];
  do_check_eq(s1_alt.min, s1.min);
  do_check_eq(s1_alt.max, s1.max);
  do_check_eq(s1_alt.histogram_type, s1.histogram_type);
  do_check_eq(s2_alt.min, s2.min);
  do_check_eq(s2_alt.max, s2.max);
  do_check_eq(s2_alt.histogram_type, s2.histogram_type);

  // Even though we've registered it, it shouldn't show up until data is added to it.
  do_check_false(name1 in snapshots[extra_addon]);

  // Flag histograms should show up automagically.
  do_check_true(flag_histogram in snapshots[flag_addon]);
  do_check_false(name2 in snapshots[flag_addon]);

  // Check that we can remove addon histograms.
  Telemetry.unregisterAddonHistograms(addon_id);
  snapshots = Telemetry.addonHistogramSnapshots;
  do_check_false(addon_id in snapshots);
  // Make sure other addons are unaffected.
  do_check_true(extra_addon in snapshots);
});

add_task(function* test_expired_histogram() {
  var test_expired_id = "TELEMETRY_TEST_EXPIRED";
  var dummy = Telemetry.getHistogramById(test_expired_id);
  var rh = Telemetry.registeredHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, []);
  Assert.ok(!!rh);

  dummy.add(1);

  do_check_eq(Telemetry.histogramSnapshots["__expired__"], undefined);
  do_check_eq(Telemetry.histogramSnapshots[test_expired_id], undefined);
  do_check_eq(rh[test_expired_id], undefined);
});

add_task(function* test_keyed_histogram() {
  // Check that invalid names get rejected.

  let threw = false;
  try {
    Telemetry.getKeyedHistogramById("test::unknown histogram", "never", Telemetry.HISTOGRAM_BOOLEAN);
  } catch (e) {
    // This should throw as it is an unknown ID
    threw = true;
  }
  Assert.ok(threw, "getKeyedHistogramById should have thrown");
});

add_task(function* test_keyed_boolean_histogram() {
  const KEYED_ID = "TELEMETRY_TEST_KEYED_BOOLEAN";
  let KEYS = numberRange(0, 2).map(i => "key" + (i + 1));
  KEYS.push("漢語");
  let histogramBase = {
    "min": 1,
    "max": 2,
    "histogram_type": 2,
    "sum": 1,
    "ranges": [0, 1, 2],
    "counts": [0, 1, 0]
  };
  let testHistograms = numberRange(0, 3).map(i => JSON.parse(JSON.stringify(histogramBase)));
  let testKeys = [];
  let testSnapShot = {};

  let h = Telemetry.getKeyedHistogramById(KEYED_ID);
  for (let i=0; i<2; ++i) {
    let key = KEYS[i];
    h.add(key, true);
    testSnapShot[key] = testHistograms[i];
    testKeys.push(key);

    Assert.deepEqual(h.keys().sort(), testKeys);
    Assert.deepEqual(h.snapshot(), testSnapShot);
  }

  h = Telemetry.getKeyedHistogramById(KEYED_ID);
  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let key = KEYS[2];
  h.add(key, false);
  testKeys.push(key);
  testSnapShot[key] = testHistograms[2];
  testSnapShot[key].sum = 0;
  testSnapShot[key].counts = [1, 0, 0];
  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let allSnapshots = Telemetry.keyedHistogramSnapshots;
  Assert.deepEqual(allSnapshots[KEYED_ID], testSnapShot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
});

add_task(function* test_keyed_count_histogram() {
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const KEYS = numberRange(0, 5).map(i => "key" + (i + 1));
  let histogramBase = {
    "min": 1,
    "max": 2,
    "histogram_type": 4,
    "sum": 0,
    "ranges": [0, 1, 2],
    "counts": [1, 0, 0]
  };
  let testHistograms = numberRange(0, 5).map(i => JSON.parse(JSON.stringify(histogramBase)));
  let testKeys = [];
  let testSnapShot = {};

  let h = Telemetry.getKeyedHistogramById(KEYED_ID);
  for (let i=0; i<4; ++i) {
    let key = KEYS[i];
    let value = i*2 + 1;

    for (let k=0; k<value; ++k) {
      h.add(key);
    }
    testHistograms[i].counts[0] = value;
    testHistograms[i].sum = value;
    testSnapShot[key] = testHistograms[i];
    testKeys.push(key);

    Assert.deepEqual(h.keys().sort(), testKeys);
    Assert.deepEqual(h.snapshot(key), testHistograms[i]);
    Assert.deepEqual(h.snapshot(), testSnapShot);
  }

  h = Telemetry.getKeyedHistogramById(KEYED_ID);
  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let key = KEYS[4];
  h.add(key);
  testKeys.push(key);
  testHistograms[4].counts[0] = 1;
  testHistograms[4].sum = 1;
  testSnapShot[key] = testHistograms[4];

  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let allSnapshots = Telemetry.keyedHistogramSnapshots;
  Assert.deepEqual(allSnapshots[KEYED_ID], testSnapShot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
});

add_task(function* test_keyed_flag_histogram() {
  const KEYED_ID = "TELEMETRY_TEST_KEYED_FLAG";
  let h = Telemetry.getKeyedHistogramById(KEYED_ID);

  const KEY = "default";
  h.add(KEY, true);

  let testSnapshot = {};
  testSnapshot[KEY] = {
    "min": 1,
    "max": 2,
    "histogram_type": 3,
    "sum": 1,
    "ranges": [0, 1, 2],
    "counts": [0, 1, 0]
  };

  Assert.deepEqual(h.keys().sort(), [KEY]);
  Assert.deepEqual(h.snapshot(), testSnapshot);

  let allSnapshots = Telemetry.keyedHistogramSnapshots;
  Assert.deepEqual(allSnapshots[KEYED_ID], testSnapshot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
});

add_task(function* test_keyed_histogram_recording() {
  // Check that no histogram is recorded if both base and extended recording are off.
  Telemetry.canRecordBase = false;
  Telemetry.canRecordExtended = false;

  const TEST_KEY = "record_foo";
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 0);

  // Check that only base histograms are recorded.
  Telemetry.canRecordBase = true;
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
               "The keyed histogram should record the correct value.");

  // Extended set keyed histograms should not be recorded.
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTIN");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 0,
               "The keyed histograms should not record any data.");

  // Check that extended histograms are recorded when required.
  Telemetry.canRecordExtended = true;

  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
                  "The runtime keyed histogram should record the correct value.");

  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTIN");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
               "The keyed histogram should record the correct value.");

  // Check that base histograms are still being recorded.
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1);
});

add_task(function* test_histogram_recording_enabled() {
  Telemetry.canRecordBase = true;
  Telemetry.canRecordExtended = true;

  // Check that a "normal" histogram respects recording-enabled on/off
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  var orig = h.snapshot();

  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
              "add should record by default.");

  // Check that when recording is disabled - add is ignored
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT", false);
  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
              "When recording is disabled add should not record.");

  // Check that we're back to normal after recording is enabled
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT", true);
  h.add(1);
  Assert.equal(orig.sum + 2, h.snapshot().sum,
               "When recording is re-enabled add should record.");

  // Check that we're correctly accumulating values other than 1.
  h.clear();
  h.add(3);
  Assert.equal(3, h.snapshot().sum, "Recording counts greater than 1 should work.");

  // Check that a histogram with recording disabled by default behaves correctly
  h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT_INIT_NO_RECORD");
  orig = h.snapshot();

  h.add(1);
  Assert.equal(orig.sum, h.snapshot().sum,
               "When recording is disabled by default, add should not record by default.");

  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT_INIT_NO_RECORD", true);
  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
               "When recording is enabled add should record.");

  // Restore to disabled
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT_INIT_NO_RECORD", false);
  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum,
               "When recording is disabled add should not record.");
});

add_task(function* test_keyed_histogram_recording_enabled() {
  Telemetry.canRecordBase = true;
  Telemetry.canRecordExtended = true;

  // Check RecordingEnabled for keyed histograms which are recording by default
  const TEST_KEY = "record_foo";
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT");

  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
    "Keyed histogram add should record by default");

  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT", false);
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
    "Keyed histogram add should not record when recording is disabled");

  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT", true);
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
    "Keyed histogram add should record when recording is re-enabled");

  // Check that a histogram with recording disabled by default behaves correctly
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD");
  h.clear();

  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 0,
    "Keyed histogram add should not record by default for histograms which don't record by default");

  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD", true);
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
    "Keyed histogram add should record when recording is enabled");

  // Restore to disabled
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD", false);
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot(TEST_KEY).sum, 1,
    "Keyed histogram add should not record when recording is disabled");
});

add_task(function* test_datasets() {
  // Check that datasets work as expected.

  const RELEASE_CHANNEL_OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
  const RELEASE_CHANNEL_OPTIN  = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;

  // Histograms should default to the extended dataset
  let h = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  Assert.equal(h.dataset(), RELEASE_CHANNEL_OPTIN);
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_FLAG");
  Assert.equal(h.dataset(), RELEASE_CHANNEL_OPTIN);

  // Check test histograms with explicit dataset definitions
  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTIN");
  Assert.equal(h.dataset(), RELEASE_CHANNEL_OPTIN);
  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTOUT");
  Assert.equal(h.dataset(), RELEASE_CHANNEL_OPTOUT);
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTIN");
  Assert.equal(h.dataset(), RELEASE_CHANNEL_OPTIN);
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT");
  Assert.equal(h.dataset(), RELEASE_CHANNEL_OPTOUT);

  // Check that registeredHistogram works properly
  let registered = Telemetry.registeredHistograms(RELEASE_CHANNEL_OPTIN, []);
  registered = new Set(registered);
  Assert.ok(registered.has("TELEMETRY_TEST_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTIN"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTOUT"));
  registered = Telemetry.registeredHistograms(RELEASE_CHANNEL_OPTOUT, []);
  registered = new Set(registered);
  Assert.ok(!registered.has("TELEMETRY_TEST_FLAG"));
  Assert.ok(!registered.has("TELEMETRY_TEST_RELEASE_OPTIN"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTOUT"));

  // Check that registeredKeyedHistograms works properly
  registered = Telemetry.registeredKeyedHistograms(RELEASE_CHANNEL_OPTIN, []);
  registered = new Set(registered);
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"));
  registered = Telemetry.registeredKeyedHistograms(RELEASE_CHANNEL_OPTOUT, []);
  registered = new Set(registered);
  Assert.ok(!registered.has("TELEMETRY_TEST_KEYED_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"));
});

add_task({
  skip_if: () => gIsAndroid
},
function* test_subsession() {
  const ID = "TELEMETRY_TEST_COUNT";
  const FLAG = "TELEMETRY_TEST_FLAG";
  let h = Telemetry.getHistogramById(ID);
  let flag = Telemetry.getHistogramById(FLAG);

  // Both original and duplicate should start out the same.
  h.clear();
  let snapshot = Telemetry.histogramSnapshots;
  let subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.ok(!(ID in snapshot));
  Assert.ok(!(ID in subsession));

  // They should instantiate and pick-up the count.
  h.add(1);
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.ok(ID in snapshot);
  Assert.ok(ID in subsession);
  Assert.equal(snapshot[ID].sum, 1);
  Assert.equal(subsession[ID].sum, 1);

  // They should still reset properly.
  h.clear();
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.ok(!(ID in snapshot));
  Assert.ok(!(ID in subsession));

  // Both should instantiate and pick-up the count.
  h.add(1);
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.equal(snapshot[ID].sum, 1);
  Assert.equal(subsession[ID].sum, 1);

  // Check that we are able to only reset the duplicate histogram.
  h.clear(true);
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.ok(ID in snapshot);
  Assert.ok(ID in subsession);
  Assert.equal(snapshot[ID].sum, 1);
  Assert.equal(subsession[ID].sum, 0);

  // Both should register the next count.
  h.add(1);
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.equal(snapshot[ID].sum, 2);
  Assert.equal(subsession[ID].sum, 1);

  // Retrieve a subsession snapshot and pass the flag to
  // clear subsession histograms too.
  h.clear();
  flag.clear();
  h.add(1);
  flag.add(1);
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms(true);
  Assert.ok(ID in snapshot);
  Assert.ok(ID in subsession);
  Assert.ok(FLAG in snapshot);
  Assert.ok(FLAG in subsession);
  Assert.equal(snapshot[ID].sum, 1);
  Assert.equal(subsession[ID].sum, 1);
  Assert.equal(snapshot[FLAG].sum, 1);
  Assert.equal(subsession[FLAG].sum, 1);

  // The next subsesssion snapshot should show the histograms
  // got reset.
  snapshot = Telemetry.histogramSnapshots;
  subsession = Telemetry.snapshotSubsessionHistograms();
  Assert.ok(ID in snapshot);
  Assert.ok(ID in subsession);
  Assert.ok(FLAG in snapshot);
  Assert.ok(FLAG in subsession);
  Assert.equal(snapshot[ID].sum, 1);
  Assert.equal(subsession[ID].sum, 0);
  Assert.equal(snapshot[FLAG].sum, 1);
  Assert.equal(subsession[FLAG].sum, 0);
});

add_task({
  skip_if: () => gIsAndroid
},
function* test_keyed_subsession() {
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_FLAG");
  const KEY = "foo";

  // Both original and subsession should start out the same.
  h.clear();
  Assert.ok(!(KEY in h.snapshot()));
  Assert.ok(!(KEY in h.subsessionSnapshot()));
  Assert.equal(h.snapshot(KEY).sum, 0);
  Assert.equal(h.subsessionSnapshot(KEY).sum, 0);

  // Both should register the flag.
  h.add(KEY, 1);
  Assert.ok(KEY in h.snapshot());
  Assert.ok(KEY in h.subsessionSnapshot());
  Assert.equal(h.snapshot(KEY).sum, 1);
  Assert.equal(h.subsessionSnapshot(KEY).sum, 1);

  // Check that we are able to only reset the subsession histogram.
  h.clear(true);
  Assert.ok(KEY in h.snapshot());
  Assert.ok(!(KEY in h.subsessionSnapshot()));
  Assert.equal(h.snapshot(KEY).sum, 1);
  Assert.equal(h.subsessionSnapshot(KEY).sum, 0);

  // Setting the flag again should make both match again.
  h.add(KEY, 1);
  Assert.ok(KEY in h.snapshot());
  Assert.ok(KEY in h.subsessionSnapshot());
  Assert.equal(h.snapshot(KEY).sum, 1);
  Assert.equal(h.subsessionSnapshot(KEY).sum, 1);

  // Check that "snapshot and clear" works properly.
  let snapshot = h.snapshot();
  let subsession = h.snapshotSubsessionAndClear();
  Assert.ok(KEY in snapshot);
  Assert.ok(KEY in subsession);
  Assert.equal(snapshot[KEY].sum, 1);
  Assert.equal(subsession[KEY].sum, 1);

  subsession = h.subsessionSnapshot();
  Assert.ok(!(KEY in subsession));
  Assert.equal(h.subsessionSnapshot(KEY).sum, 0);
});
