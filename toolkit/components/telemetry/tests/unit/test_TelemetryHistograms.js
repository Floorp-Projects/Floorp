/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INT_MAX = 0x7FFFFFFF;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);

// Return an array of numbers from lower up to, excluding, upper
function numberRange(lower, upper) {
  let a = [];
  for (let i = lower; i < upper; ++i) {
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
  for (let i = 0;i < r.length;i++) {
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
  var hgrams = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            false,
                                            false).parent;
  let gh = hgrams[name];
  do_check_eq(gh.histogram_type, histogram_type);

  do_check_eq(gh.min, min);
  do_check_eq(gh.max, max);

  // Check that booleans work with nonboolean histograms
  h.add(false);
  h.add(true);
  s = h.snapshot().counts;
  do_check_eq(s[0], 2);
  do_check_eq(s[1], 2);

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
function test_instantiate() {
  const ID = "TELEMETRY_TEST_COUNT";
  let h = Telemetry.getHistogramById(ID);

  // Instantiate the subsession histogram through |add| and make sure they match.
  // This MUST be the first use of "TELEMETRY_TEST_COUNT" in this file, otherwise
  // |add| will not instantiate the histogram.
  h.add(1);
  let snapshot = h.snapshot();
  let subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                true /* subsession */,
                                                false /* clear */).parent;
  Assert.ok(ID in subsession);
  Assert.equal(snapshot.sum, subsession[ID].sum,
               "Histogram and subsession histogram sum must match.");
  // Clear the histogram, so we don't void the assumptions from the other tests.
  h.clear();
});

add_task(async function test_parameterChecks() {
  let kinds = [Telemetry.HISTOGRAM_EXPONENTIAL, Telemetry.HISTOGRAM_LINEAR];
  let testNames = ["TELEMETRY_TEST_EXPONENTIAL", "TELEMETRY_TEST_LINEAR"];
  for (let i = 0; i < kinds.length; i++) {
    let histogram_type = kinds[i];
    let test_type = testNames[i];
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10];
    check_histogram(histogram_type, test_type, min, max, bucket_count);
  }
});

add_task(async function test_parameterCounts() {
  let histogramIds = [
    "TELEMETRY_TEST_EXPONENTIAL",
    "TELEMETRY_TEST_LINEAR",
    "TELEMETRY_TEST_FLAG",
    "TELEMETRY_TEST_CATEGORICAL",
    "TELEMETRY_TEST_BOOLEAN",
  ];

  for (let id of histogramIds) {
    let h = Telemetry.getHistogramById(id);
    h.clear();
    h.add();
    Assert.equal(h.snapshot().sum, 0, "Calling add() without a value should only log an error.");
    h.clear();
  }
});

add_task(async function test_parameterCountsKeyed() {
  let histogramIds = [
    "TELEMETRY_TEST_KEYED_FLAG",
    "TELEMETRY_TEST_KEYED_BOOLEAN",
    "TELEMETRY_TEST_KEYED_EXPONENTIAL",
    "TELEMETRY_TEST_KEYED_LINEAR",
  ];

  for (let id of histogramIds) {
    let h = Telemetry.getKeyedHistogramById(id);
    h.clear();
    h.add("key");
    Assert.equal(h.snapshot("key").sum, 0, "Calling add('key') without a value should only log an error.");
    h.clear();
  }
});

add_task(async function test_noSerialization() {
  // Instantiate the storage for this histogram and make sure it doesn't
  // get reflected into JS, as it has no interesting data in it.
  Telemetry.getHistogramById("NEWTAB_PAGE_PINNED_SITES_COUNT");
  let histograms = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                false /* subsession */,
                                                false /* clear */).parent;
  do_check_false("NEWTAB_PAGE_PINNED_SITES_COUNT" in histograms);
});

add_task(async function test_boolean_histogram() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var r = h.snapshot().ranges;
  // boolean histograms ignore numeric parameters
  do_check_eq(uneval(r), uneval([0, 1, 2]));
  for (var i = 0;i < r.length;i++) {
    var v = r[i];
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

add_task(async function test_flag_histogram() {
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

add_task(async function test_count_histogram() {
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

add_task(async function test_categorical_histogram() {
  let h1 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL");
  for (let v of ["CommonLabel", "Label2", "Label3", "Label3", 0, 0, 1]) {
    h1.add(v);
  }
  for (let s of ["", "Label4", "1234"]) {
    // The |add| method should not throw for unexpected values, but rather
    // print an error message in the console.
    h1.add(s);
  }

  // Categorical histograms default to 50 linear buckets.
  let expectedRanges = [];
  for (let i = 0; i < 51; ++i) {
    expectedRanges.push(i);
  }

  let snapshot = h1.snapshot();
  Assert.equal(snapshot.sum, 6);
  Assert.deepEqual(snapshot.ranges, expectedRanges);
  Assert.deepEqual(snapshot.counts.slice(0, 4), [3, 2, 2, 0]);

  let h2 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL_OPTOUT");
  for (let v of ["CommonLabel", "CommonLabel", "Label4", "Label5", "Label6", 0, 1]) {
    h2.add(v);
  }
  for (let s of ["", "Label3", "1234"]) {
    // The |add| method should not throw for unexpected values, but rather
    // print an error message in the console.
    h2.add(s);
  }

  snapshot = h2.snapshot();
  Assert.equal(snapshot.sum, 7);
  Assert.deepEqual(snapshot.ranges, expectedRanges);
  Assert.deepEqual(snapshot.counts.slice(0, 5), [3, 2, 1, 1, 0]);

  // This histogram overrides the default of 50 values to 70.
  let h3 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL_NVALUES");
  for (let v of ["CommonLabel", "Label7", "Label8"]) {
    h3.add(v);
  }

  expectedRanges = [];
  for (let i = 0; i < 71; ++i) {
    expectedRanges.push(i);
  }

  snapshot = h3.snapshot();
  Assert.equal(snapshot.sum, 3);
  Assert.equal(snapshot.ranges.length, expectedRanges.length);
  Assert.deepEqual(snapshot.ranges, expectedRanges);
  Assert.deepEqual(snapshot.counts.slice(0, 4), [1, 1, 1, 0]);
});

add_task(async function test_add_error_behaviour() {
  const PLAIN_HISTOGRAMS_TO_TEST = [
    "TELEMETRY_TEST_FLAG",
    "TELEMETRY_TEST_EXPONENTIAL",
    "TELEMETRY_TEST_LINEAR",
    "TELEMETRY_TEST_BOOLEAN"
  ];

  const KEYED_HISTOGRAMS_TO_TEST = [
    "TELEMETRY_TEST_KEYED_FLAG",
    "TELEMETRY_TEST_KEYED_COUNT",
    "TELEMETRY_TEST_KEYED_BOOLEAN"
  ];

  // Check that |add| doesn't throw for plain histograms.
  for (let hist of PLAIN_HISTOGRAMS_TO_TEST) {
    const returnValue = Telemetry.getHistogramById(hist).add("unexpected-value");
    Assert.strictEqual(returnValue, undefined,
                       "Adding to an histogram must return 'undefined'.");
  }

  // And for keyed histograms.
  for (let hist of KEYED_HISTOGRAMS_TO_TEST) {
    const returnValue = Telemetry.getKeyedHistogramById(hist).add("some-key", "unexpected-value");
    Assert.strictEqual(returnValue, undefined,
                       "Adding to a keyed histogram must return 'undefined'.");
  }
});

add_task(async function test_API_return_values() {
  // Check that the plain scalar functions don't allow to crash the browser.
  // We expect 'undefined' to be returned so that .add(1).add() can't be called.
  // See bug 1321349 for context.
  let hist = Telemetry.getHistogramById("TELEMETRY_TEST_LINEAR");
  let keyedHist = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");

  const RETURN_VALUES = [
    hist.clear(),
    hist.add(1),
    keyedHist.clear(),
    keyedHist.add("some-key", 1),
  ];

  for (let returnValue of RETURN_VALUES) {
    Assert.strictEqual(returnValue, undefined,
                       "The function must return undefined");
  }
});

add_task(async function test_getHistogramById() {
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

add_task(async function test_getSlowSQL() {
  var slow = Telemetry.slowSQL;
  do_check_true(("mainThread" in slow) && ("otherThreads" in slow));
});

add_task(async function test_getWebrtc() {
  var webrtc = Telemetry.webrtcStats;
  do_check_true("IceCandidatesStats" in webrtc);
  var icestats = webrtc.IceCandidatesStats;
  do_check_true("webrtc" in icestats);
});

// Check that telemetry doesn't record in private mode
add_task(async function test_privateMode() {
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
add_task(async function test_histogramRecording() {
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

add_task(async function test_expired_histogram() {
  var test_expired_id = "TELEMETRY_TEST_EXPIRED";
  var dummy = Telemetry.getHistogramById(test_expired_id);

  dummy.add(1);

  for (let process of ["main", "content", "gpu", "extension"]) {
    let histograms = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                  false /* subsession */,
                                                  false /* clear */);
    if (!(process in histograms)) {
      do_print("Nothing present for process " + process);
      continue;
    }
    do_check_eq(histograms[process].__expired__, undefined);
  }
  let parentHgrams = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                  false /* subsession */,
                                                  false /* clear */).parent;
  do_check_eq(parentHgrams[test_expired_id], undefined);
});

add_task(async function test_keyed_histogram() {
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

add_task(async function test_keyed_boolean_histogram() {
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
  for (let i = 0; i < 2; ++i) {
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

  let parentHgrams = Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                       false /* subsession */,
                                                       false /* clear */).parent;
  Assert.deepEqual(parentHgrams[KEYED_ID], testSnapShot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
});

add_task(async function test_keyed_count_histogram() {
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
  h.clear();
  for (let i = 0; i < 4; ++i) {
    let key = KEYS[i];
    let value = i * 2 + 1;

    for (let k = 0; k < value; ++k) {
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

  let parentHgrams = Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                       false /* subsession */,
                                                       false /* clear */).parent;
  Assert.deepEqual(parentHgrams[KEYED_ID], testSnapShot);

  // Test clearing categorical histogram.
  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});

  // Test leaving out the value argument. That should increment by 1.
  h.add("key");
  Assert.equal(h.snapshot("key").sum, 1);
});

add_task(async function test_keyed_categorical_histogram() {
  const KEYED_ID = "TELEMETRY_TEST_KEYED_CATEGORICAL";
  const KEYS = numberRange(0, 5).map(i => "key" + (i + 1));

  let h = Telemetry.getKeyedHistogramById(KEYED_ID);

  for (let k of KEYS) {
    // Test adding both per label and index.
    for (let v of ["CommonLabel", "Label2", "Label3", "Label3", 0, 0, 1]) {
      h.add(k, v);
    }

    // The |add| method should not throw for unexpected values, but rather
    // print an error message in the console.
    for (let s of ["", "Label4", "1234"]) {
      h.add(k, s);
    }
  }

  // Categorical histograms default to 50 linear buckets.
  let expectedRanges = [];
  for (let i = 0; i < 51; ++i) {
    expectedRanges.push(i);
  }

  // Check that the set of keys in the snapshot is what we expect.
  let snapshot = h.snapshot();
  let snapshotKeys = Object.keys(snapshot);
  Assert.equal(KEYS.length, snapshotKeys.length);
  Assert.ok(KEYS.every(k => snapshotKeys.includes(k)));

  // Check the snapshot values.
  for (let k of KEYS) {
    Assert.ok(k in snapshot);
    Assert.equal(snapshot[k].sum, 6);
    Assert.deepEqual(snapshot[k].ranges, expectedRanges);
    Assert.deepEqual(snapshot[k].counts.slice(0, 4), [3, 2, 2, 0]);
  }
});

add_task(async function test_keyed_flag_histogram() {
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

  let parentHgrams = Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                       false /* subsession */,
                                                       false /* clear */).parent;
  Assert.deepEqual(parentHgrams[KEYED_ID], testSnapshot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
});

add_task(async function test_keyed_histogram_recording() {
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

add_task(async function test_histogram_recording_enabled() {
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

add_task(async function test_keyed_histogram_recording_enabled() {
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

add_task(async function test_histogramSnapshots() {
  let keyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  keyed.add("a", 1);

  // Check that keyed histograms are not returned
  let parentHgrams = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                  false /* subsession */,
                                                  false /* clear */).parent;
  Assert.ok(!("TELEMETRY_TEST_KEYED_COUNT" in parentHgrams));
});

add_task(async function test_datasets() {
  // Check that datasets work as expected.

  const RELEASE_CHANNEL_OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
  const RELEASE_CHANNEL_OPTIN  = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;

  // Check that registeredHistogram works properly
  let registered = Telemetry.snapshotHistograms(RELEASE_CHANNEL_OPTIN,
                                                false /* subsession */,
                                                false /* clear */);
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(registered.has("TELEMETRY_TEST_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTIN"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTOUT"));
  registered = Telemetry.snapshotHistograms(RELEASE_CHANNEL_OPTOUT,
                                            false /* subsession */,
                                            false /* clear */);
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(!registered.has("TELEMETRY_TEST_FLAG"));
  Assert.ok(!registered.has("TELEMETRY_TEST_RELEASE_OPTIN"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTOUT"));

  // Check that registeredKeyedHistograms works properly
  registered = Telemetry.snapshotKeyedHistograms(RELEASE_CHANNEL_OPTIN,
                                                 false /* subsession */,
                                                 false /* clear */);
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"));
  registered = Telemetry.snapshotKeyedHistograms(RELEASE_CHANNEL_OPTOUT,
                                                 false /* subsession */,
                                                 false /* clear */);
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(!registered.has("TELEMETRY_TEST_KEYED_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"));
});

add_task({
  skip_if: () => gIsAndroid
},
function test_subsession() {
  const COUNT = "TELEMETRY_TEST_COUNT";
  const FLAG = "TELEMETRY_TEST_FLAG";
  let h = Telemetry.getHistogramById(COUNT);
  let flag = Telemetry.getHistogramById(FLAG);

  // Both original and duplicate should start out the same.
  h.clear();
  let snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                              false /* subsession */,
                                              false /* clear */).parent;
  let subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                true /* subsession */,
                                                false /* clear */).parent;
  Assert.ok(!(COUNT in snapshot));
  Assert.ok(!(COUNT in subsession));

  // They should instantiate and pick-up the count.
  h.add(1);
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            false /* clear */).parent;
  Assert.ok(COUNT in snapshot);
  Assert.ok(COUNT in subsession);
  Assert.equal(snapshot[COUNT].sum, 1);
  Assert.equal(subsession[COUNT].sum, 1);

  // They should still reset properly.
  h.clear();
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            false /* clear */).parent;
  Assert.ok(!(COUNT in snapshot));
  Assert.ok(!(COUNT in subsession));

  // Both should instantiate and pick-up the count.
  h.add(1);
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            false /* clear */).parent;
  Assert.ok(COUNT in snapshot);
  Assert.ok(COUNT in subsession);
  Assert.equal(snapshot[COUNT].sum, 1);
  Assert.equal(subsession[COUNT].sum, 1);

  // Check that we are able to only reset the duplicate histogram.
  h.clear(true);
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            false /* clear */).parent;
  Assert.ok(COUNT in snapshot);
  Assert.ok(!(COUNT in subsession));
  Assert.equal(snapshot[COUNT].sum, 1);

  // Both should register the next count.
  h.add(1);
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            false /* clear */).parent;
  Assert.equal(snapshot[COUNT].sum, 2);
  Assert.equal(subsession[COUNT].sum, 1);

  // Retrieve a subsession snapshot and pass the flag to
  // clear subsession histograms too.
  h.clear();
  flag.clear();
  h.add(1);
  flag.add(1);
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            true /* clear */).parent;
  Assert.ok(COUNT in snapshot);
  Assert.ok(COUNT in subsession);
  Assert.ok(FLAG in snapshot);
  Assert.ok(FLAG in subsession);
  Assert.equal(snapshot[COUNT].sum, 1);
  Assert.equal(subsession[COUNT].sum, 1);
  Assert.equal(snapshot[FLAG].sum, 1);
  Assert.equal(subsession[FLAG].sum, 1);

  // The next subsesssion snapshot should show the histograms
  // got reset.
  snapshot = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                          false /* subsession */,
                                          false /* clear */).parent;
  subsession = Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                            true /* subsession */,
                                            false /* clear */).parent;
  Assert.ok(COUNT in snapshot);
  Assert.ok(!(COUNT in subsession));
  Assert.ok(FLAG in snapshot);
  Assert.ok(FLAG in subsession);
  Assert.equal(snapshot[COUNT].sum, 1);
  Assert.equal(snapshot[FLAG].sum, 1);
  Assert.equal(subsession[FLAG].sum, 0);
});

add_task({
  skip_if: () => gIsAndroid
},
function test_keyed_subsession() {
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

add_task(function* test_keyed_keys() {
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_KEYS");
  h.clear();
  Telemetry.clearScalars();

  // The |add| method should not throw for keys that are not allowed.
  h.add("testkey", true);
  h.add("thirdKey", false);
  h.add("not-allowed", true);

  // Check that we have the expected keys.
  let snap = h.snapshot();
  Assert.ok(Object.keys(snap).length, 2, "Only 2 keys must be recorded.");
  Assert.ok("testkey" in snap, "'testkey' must be recorded.");
  Assert.ok("thirdKey" in snap, "'thirdKey' must be recorded.");
  Assert.deepEqual(snap.testkey.counts, [0, 1, 0],
                   "'testkey' must contain the correct value.");
  Assert.deepEqual(snap.thirdKey.counts, [1, 0, 0],
                   "'thirdKey' must contain the correct value.");

  // Keys that are not allowed must not be recorded.
  Assert.ok(!("not-allowed" in snap), "'not-allowed' must not be recorded.");

  // Check that these failures were correctly tracked.
  const parentScalars =
    Telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false).parent;
  const scalarName = "telemetry.accumulate_unknown_histogram_keys";
  Assert.ok(scalarName in parentScalars, "Accumulation to unallowed keys must be reported.");
  Assert.ok("TELEMETRY_TEST_KEYED_KEYS" in parentScalars[scalarName],
            "Accumulation to unallowed keys must be recorded with the correct key.");
  Assert.equal(parentScalars[scalarName].TELEMETRY_TEST_KEYED_KEYS, 1,
               "Accumulation to unallowed keys must report the correct value.");
});
