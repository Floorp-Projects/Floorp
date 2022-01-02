/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INT_MAX = 0x7fffffff;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);

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
  Assert.ok(failed);
}

function expect_success(f) {
  let succeeded = false;
  try {
    f();
    succeeded = true;
  } catch (e) {
    succeeded = false;
  }
  Assert.ok(succeeded);
}

function check_histogram(histogram_type, name, min, max, bucket_count) {
  var h = Telemetry.getHistogramById(name);
  h.add(0);
  var s = h.snapshot();
  Assert.equal(0, s.sum);

  var hgrams = Telemetry.getSnapshotForHistograms("main", false).parent;
  let gh = hgrams[name];
  Assert.equal(gh.histogram_type, histogram_type);

  Assert.deepEqual(gh.range, [min, max]);

  // Check that booleans work with nonboolean histograms
  h.add(false);
  h.add(true);
  s = Object.values(h.snapshot().values);
  Assert.deepEqual(s, [2, 1, 0]);

  // Check that clearing works.
  h.clear();
  s = h.snapshot();
  Assert.deepEqual(s.values, {});
  Assert.equal(s.sum, 0);

  h.add(0);
  h.add(1);
  var c = Object.values(h.snapshot().values);
  Assert.deepEqual(c, [1, 1, 0]);
}

// This MUST be the very first test of this file.
add_task(
  {
    skip_if: () => gIsAndroid,
  },
  function test_instantiate() {
    const ID = "TELEMETRY_TEST_COUNT";
    let h = Telemetry.getHistogramById(ID);

    // Instantiate the subsession histogram through |add| and make sure they match.
    // This MUST be the first use of "TELEMETRY_TEST_COUNT" in this file, otherwise
    // |add| will not instantiate the histogram.
    h.add(1);
    let snapshot = h.snapshot();
    let subsession = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;
    Assert.ok(ID in subsession);
    Assert.equal(
      snapshot.sum,
      subsession[ID].sum,
      "Histogram and subsession histogram sum must match."
    );
    // Clear the histogram, so we don't void the assumptions from the other tests.
    h.clear();
  }
);

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
    Assert.equal(
      h.snapshot().sum,
      0,
      "Calling add() without a value should only log an error."
    );
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
    Assert.deepEqual(
      h.snapshot(),
      {},
      "Calling add('key') without a value should only log an error."
    );
    h.clear();
  }
});

add_task(async function test_noSerialization() {
  // Instantiate the storage for this histogram and make sure it doesn't
  // get reflected into JS, as it has no interesting data in it.
  Telemetry.getHistogramById("NEWTAB_PAGE_PINNED_SITES_COUNT");
  let histograms = Telemetry.getSnapshotForHistograms("main", false /* clear */)
    .parent;
  Assert.equal(false, "NEWTAB_PAGE_PINNED_SITES_COUNT" in histograms);
});

add_task(async function test_boolean_histogram() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var r = h.snapshot().range;
  // boolean histograms ignore numeric parameters
  Assert.deepEqual(r, [1, 2]);
  h.add(0);
  h.add(1);
  h.add(2);

  h.add(true);
  h.add(false);
  var s = h.snapshot();
  Assert.equal(s.histogram_type, Telemetry.HISTOGRAM_BOOLEAN);
  // last bucket should always be 0 since .add parameters are normalized to either 0 or 1
  Assert.deepEqual(s.values, { 0: 2, 1: 3, 2: 0 });
  Assert.equal(s.sum, 3);
});

add_task(async function test_flag_histogram() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  var r = h.snapshot().range;
  // Flag histograms ignore numeric parameters.
  Assert.deepEqual(r, [1, 2]);
  // Should already have a 0 counted.
  var v = h.snapshot().values;
  var s = h.snapshot().sum;
  Assert.deepEqual(v, { 0: 1, 1: 0 });
  Assert.equal(s, 0);
  // Should switch counts.
  h.add(1);
  var v2 = h.snapshot().values;
  var s2 = h.snapshot().sum;
  Assert.deepEqual(v2, { 0: 0, 1: 1, 2: 0 });
  Assert.equal(s2, 1);
  // Should only switch counts once.
  h.add(1);
  var v3 = h.snapshot().values;
  var s3 = h.snapshot().sum;
  Assert.deepEqual(v3, { 0: 0, 1: 1, 2: 0 });
  Assert.equal(s3, 1);
  Assert.equal(h.snapshot().histogram_type, Telemetry.HISTOGRAM_FLAG);
});

add_task(async function test_count_histogram() {
  let h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT2");
  let s = h.snapshot();
  Assert.deepEqual(s.range, [1, 2]);
  Assert.deepEqual(s.values, {});
  Assert.equal(s.sum, 0);
  h.add();
  s = h.snapshot();
  Assert.deepEqual(s.values, { 0: 1, 1: 0 });
  Assert.equal(s.sum, 1);
  h.add();
  s = h.snapshot();
  Assert.deepEqual(s.values, { 0: 2, 1: 0 });
  Assert.equal(s.sum, 2);
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

  let snapshot = h1.snapshot();
  Assert.equal(snapshot.sum, 6);
  Assert.deepEqual(snapshot.range, [1, 50]);
  Assert.deepEqual(snapshot.values, { 0: 3, 1: 2, 2: 2, 3: 0 });

  let h2 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL_OPTOUT");
  for (let v of [
    "CommonLabel",
    "CommonLabel",
    "Label4",
    "Label5",
    "Label6",
    0,
    1,
  ]) {
    h2.add(v);
  }
  for (let s of ["", "Label3", "1234"]) {
    // The |add| method should not throw for unexpected values, but rather
    // print an error message in the console.
    h2.add(s);
  }

  snapshot = h2.snapshot();
  Assert.equal(snapshot.sum, 7);
  Assert.deepEqual(snapshot.range, [1, 50]);
  Assert.deepEqual(snapshot.values, { 0: 3, 1: 2, 2: 1, 3: 1, 4: 0 });

  // This histogram overrides the default of 50 values to 70.
  let h3 = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL_NVALUES");
  for (let v of ["CommonLabel", "Label7", "Label8"]) {
    h3.add(v);
  }

  snapshot = h3.snapshot();
  Assert.equal(snapshot.sum, 3);
  Assert.deepEqual(snapshot.range, [1, 70]);
  Assert.deepEqual(snapshot.values, { 0: 1, 1: 1, 2: 1, 3: 0 });
});

add_task(async function test_getCategoricalLabels() {
  let h = Telemetry.getCategoricalLabels();

  Assert.deepEqual(h.TELEMETRY_TEST_CATEGORICAL, [
    "CommonLabel",
    "Label2",
    "Label3",
  ]);
  Assert.deepEqual(h.TELEMETRY_TEST_CATEGORICAL_OPTOUT, [
    "CommonLabel",
    "Label4",
    "Label5",
    "Label6",
  ]);
  Assert.deepEqual(h.TELEMETRY_TEST_CATEGORICAL_NVALUES, [
    "CommonLabel",
    "Label7",
    "Label8",
  ]);
  Assert.deepEqual(h.TELEMETRY_TEST_KEYED_CATEGORICAL, [
    "CommonLabel",
    "Label2",
    "Label3",
  ]);
});

add_task(async function test_add_error_behaviour() {
  const PLAIN_HISTOGRAMS_TO_TEST = [
    "TELEMETRY_TEST_FLAG",
    "TELEMETRY_TEST_EXPONENTIAL",
    "TELEMETRY_TEST_LINEAR",
    "TELEMETRY_TEST_BOOLEAN",
  ];

  const KEYED_HISTOGRAMS_TO_TEST = [
    "TELEMETRY_TEST_KEYED_FLAG",
    "TELEMETRY_TEST_KEYED_COUNT",
    "TELEMETRY_TEST_KEYED_BOOLEAN",
  ];

  // Check that |add| doesn't throw for plain histograms.
  for (let hist of PLAIN_HISTOGRAMS_TO_TEST) {
    const returnValue = Telemetry.getHistogramById(hist).add(
      "unexpected-value"
    );
    Assert.strictEqual(
      returnValue,
      undefined,
      "Adding to an histogram must return 'undefined'."
    );
  }

  // And for keyed histograms.
  for (let hist of KEYED_HISTOGRAMS_TO_TEST) {
    const returnValue = Telemetry.getKeyedHistogramById(hist).add(
      "some-key",
      "unexpected-value"
    );
    Assert.strictEqual(
      returnValue,
      undefined,
      "Adding to a keyed histogram must return 'undefined'."
    );
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
    Assert.strictEqual(
      returnValue,
      undefined,
      "The function must return undefined"
    );
  }
});

add_task(async function test_getHistogramById() {
  try {
    Telemetry.getHistogramById("nonexistent");
    do_throw("This can't happen");
  } catch (e) {}
  var h = Telemetry.getHistogramById("CYCLE_COLLECTOR");
  var s = h.snapshot();
  Assert.equal(s.histogram_type, Telemetry.HISTOGRAM_EXPONENTIAL);
  Assert.deepEqual(s.range, [1, 10000]);
});

add_task(async function test_getSlowSQL() {
  var slow = Telemetry.slowSQL;
  Assert.ok("mainThread" in slow && "otherThreads" in slow);
});

// Check that telemetry doesn't record in private mode
add_task(async function test_privateMode() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var orig = h.snapshot();
  Telemetry.canRecordExtended = false;
  h.add(1);
  Assert.deepEqual(orig, h.snapshot());
  Telemetry.canRecordExtended = true;
  h.add(1);
  Assert.notDeepEqual(orig, h.snapshot());
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
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "Histogram value should have incremented by 1 due to recording."
  );

  // Extended histograms should not be recorded.
  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTIN");
  orig = h.snapshot();
  h.add(1);
  Assert.equal(
    orig.sum,
    h.snapshot().sum,
    "Histograms should be equal after recording."
  );

  // Runtime created histograms should not be recorded.
  h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  orig = h.snapshot();
  h.add(1);
  Assert.equal(
    orig.sum,
    h.snapshot().sum,
    "Histograms should be equal after recording."
  );

  // Check that extended histograms are recorded when required.
  Telemetry.canRecordExtended = true;

  h.add(1);
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "Runtime histogram value should have incremented by 1 due to recording."
  );

  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTIN");
  orig = h.snapshot();
  h.add(1);
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "Histogram value should have incremented by 1 due to recording."
  );

  // Check that base histograms are still being recorded.
  h = Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTOUT");
  h.clear();
  orig = h.snapshot();
  h.add(1);
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "Histogram value should have incremented by 1 due to recording."
  );
});

add_task(async function test_expired_histogram() {
  var test_expired_id = "TELEMETRY_TEST_EXPIRED";
  var dummy = Telemetry.getHistogramById(test_expired_id);

  dummy.add(1);

  for (let process of ["main", "content", "gpu", "extension"]) {
    let histograms = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    );
    if (!(process in histograms)) {
      info("Nothing present for process " + process);
      continue;
    }
    Assert.equal(histograms[process].__expired__, undefined);
  }
  let parentHgrams = Telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;
  Assert.equal(parentHgrams[test_expired_id], undefined);
});

add_task(async function test_keyed_expired_histogram() {
  var test_expired_id = "TELEMETRY_TEST_EXPIRED_KEYED";
  var dummy = Telemetry.getKeyedHistogramById(test_expired_id);
  dummy.add("someKey", 1);

  const histograms = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  );
  for (let process of ["parent", "content", "gpu", "extension"]) {
    if (!(process in histograms)) {
      info("Nothing present for process " + process);
      continue;
    }
    Assert.ok(
      !(test_expired_id in histograms[process]),
      "The expired keyed histogram must not be reported"
    );
  }
});

add_task(async function test_keyed_histogram() {
  // Check that invalid names get rejected.

  let threw = false;
  try {
    Telemetry.getKeyedHistogramById(
      "test::unknown histogram",
      "never",
      Telemetry.HISTOGRAM_BOOLEAN
    );
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
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 2,
    sum: 1,
    values: { 0: 0, 1: 1, 2: 0 },
  };
  let testHistograms = numberRange(0, 3).map(i =>
    JSON.parse(JSON.stringify(histogramBase))
  );
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
  testSnapShot[key].values = { 0: 1, 1: 0 };
  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let parentHgrams = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  ).parent;
  Assert.deepEqual(parentHgrams[KEYED_ID], testSnapShot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
});

add_task(async function test_keyed_count_histogram() {
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const KEYS = numberRange(0, 5).map(i => "key" + (i + 1));
  let histogramBase = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 4,
    sum: 0,
    values: { 0: 1, 1: 0 },
  };
  let testHistograms = numberRange(0, 5).map(i =>
    JSON.parse(JSON.stringify(histogramBase))
  );
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
    testHistograms[i].values[0] = value;
    testHistograms[i].sum = value;
    testSnapShot[key] = testHistograms[i];
    testKeys.push(key);

    Assert.deepEqual(h.keys().sort(), testKeys);
    Assert.deepEqual(h.snapshot()[key], testHistograms[i]);
    Assert.deepEqual(h.snapshot(), testSnapShot);
  }

  h = Telemetry.getKeyedHistogramById(KEYED_ID);
  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let key = KEYS[4];
  h.add(key);
  testKeys.push(key);
  testHistograms[4].values[0] = 1;
  testHistograms[4].sum = 1;
  testSnapShot[key] = testHistograms[4];

  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let parentHgrams = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  ).parent;
  Assert.deepEqual(parentHgrams[KEYED_ID], testSnapShot);

  // Test clearing categorical histogram.
  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});

  // Test leaving out the value argument. That should increment by 1.
  h.add("key");
  Assert.equal(h.snapshot().key.sum, 1);
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

  // Check that the set of keys in the snapshot is what we expect.
  let snapshot = h.snapshot();
  let snapshotKeys = Object.keys(snapshot);
  Assert.equal(KEYS.length, snapshotKeys.length);
  Assert.ok(KEYS.every(k => snapshotKeys.includes(k)));

  // Check the snapshot values.
  for (let k of KEYS) {
    Assert.ok(k in snapshot);
    Assert.equal(snapshot[k].sum, 6);
    Assert.deepEqual(snapshot[k].range, [1, 50]);
    Assert.deepEqual(snapshot[k].values, { 0: 3, 1: 2, 2: 2, 3: 0 });
  }
});

add_task(async function test_keyed_flag_histogram() {
  const KEYED_ID = "TELEMETRY_TEST_KEYED_FLAG";
  let h = Telemetry.getKeyedHistogramById(KEYED_ID);

  const KEY = "default";
  h.add(KEY, true);

  let testSnapshot = {};
  testSnapshot[KEY] = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 3,
    sum: 1,
    values: { 0: 0, 1: 1, 2: 0 },
  };

  Assert.deepEqual(h.keys().sort(), [KEY]);
  Assert.deepEqual(h.snapshot(), testSnapshot);

  let parentHgrams = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  ).parent;
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
  let h = Telemetry.getKeyedHistogramById(
    "TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"
  );
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.ok(!(TEST_KEY in h.snapshot()));

  // Check that only base histograms are recorded.
  Telemetry.canRecordBase = true;
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "The keyed histogram should record the correct value."
  );

  // Extended set keyed histograms should not be recorded.
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTIN");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.ok(
    !(TEST_KEY in h.snapshot()),
    "The keyed histograms should not record any data."
  );

  // Check that extended histograms are recorded when required.
  Telemetry.canRecordExtended = true;

  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "The runtime keyed histogram should record the correct value."
  );

  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTIN");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "The keyed histogram should record the correct value."
  );

  // Check that base histograms are still being recorded.
  h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT");
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(h.snapshot()[TEST_KEY].sum, 1);
});

add_task(async function test_histogram_recording_enabled() {
  Telemetry.canRecordBase = true;
  Telemetry.canRecordExtended = true;

  // Check that a "normal" histogram respects recording-enabled on/off
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  var orig = h.snapshot();

  h.add(1);
  Assert.equal(orig.sum + 1, h.snapshot().sum, "add should record by default.");

  // Check that when recording is disabled - add is ignored
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT", false);
  h.add(1);
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "When recording is disabled add should not record."
  );

  // Check that we're back to normal after recording is enabled
  Telemetry.setHistogramRecordingEnabled("TELEMETRY_TEST_COUNT", true);
  h.add(1);
  Assert.equal(
    orig.sum + 2,
    h.snapshot().sum,
    "When recording is re-enabled add should record."
  );

  // Check that we're correctly accumulating values other than 1.
  h.clear();
  h.add(3);
  Assert.equal(
    3,
    h.snapshot().sum,
    "Recording counts greater than 1 should work."
  );

  // Check that a histogram with recording disabled by default behaves correctly
  h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT_INIT_NO_RECORD");
  orig = h.snapshot();

  h.add(1);
  Assert.equal(
    orig.sum,
    h.snapshot().sum,
    "When recording is disabled by default, add should not record by default."
  );

  Telemetry.setHistogramRecordingEnabled(
    "TELEMETRY_TEST_COUNT_INIT_NO_RECORD",
    true
  );
  h.add(1);
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "When recording is enabled add should record."
  );

  // Restore to disabled
  Telemetry.setHistogramRecordingEnabled(
    "TELEMETRY_TEST_COUNT_INIT_NO_RECORD",
    false
  );
  h.add(1);
  Assert.equal(
    orig.sum + 1,
    h.snapshot().sum,
    "When recording is disabled add should not record."
  );
});

add_task(async function test_keyed_histogram_recording_enabled() {
  Telemetry.canRecordBase = true;
  Telemetry.canRecordExtended = true;

  // Check RecordingEnabled for keyed histograms which are recording by default
  const TEST_KEY = "record_foo";
  let h = Telemetry.getKeyedHistogramById(
    "TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"
  );

  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "Keyed histogram add should record by default"
  );

  Telemetry.setHistogramRecordingEnabled(
    "TELEMETRY_TEST_KEYED_RELEASE_OPTOUT",
    false
  );
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "Keyed histogram add should not record when recording is disabled"
  );

  Telemetry.setHistogramRecordingEnabled(
    "TELEMETRY_TEST_KEYED_RELEASE_OPTOUT",
    true
  );
  h.clear();
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "Keyed histogram add should record when recording is re-enabled"
  );

  // Check that a histogram with recording disabled by default behaves correctly
  h = Telemetry.getKeyedHistogramById(
    "TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD"
  );
  h.clear();

  h.add(TEST_KEY, 1);
  Assert.ok(
    !(TEST_KEY in h.snapshot()),
    "Keyed histogram add should not record by default for histograms which don't record by default"
  );

  Telemetry.setHistogramRecordingEnabled(
    "TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD",
    true
  );
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "Keyed histogram add should record when recording is enabled"
  );

  // Restore to disabled
  Telemetry.setHistogramRecordingEnabled(
    "TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD",
    false
  );
  h.add(TEST_KEY, 1);
  Assert.equal(
    h.snapshot()[TEST_KEY].sum,
    1,
    "Keyed histogram add should not record when recording is disabled"
  );
});

add_task(async function test_histogramSnapshots() {
  let keyed = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  keyed.add("a", 1);

  // Check that keyed histograms are not returned
  let parentHgrams = Telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;
  Assert.ok(!("TELEMETRY_TEST_KEYED_COUNT" in parentHgrams));
});

add_task(async function test_datasets() {
  // Check that datasets work as expected.

  const currentRecordExtended = Telemetry.canRecordExtended;

  // Clear everything out
  Telemetry.getSnapshotForHistograms("main", true /* clear */);
  Telemetry.getSnapshotForKeyedHistograms("main", true /* clear */);

  // Empty histograms are filtered. Let's record what we check below.
  Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTIN").add(1);
  Telemetry.getHistogramById("TELEMETRY_TEST_RELEASE_OPTOUT").add(1);
  // Keyed flag histograms are skipped if empty, let's add data
  Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_FLAG").add("a", 1);
  Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTIN").add(
    "a",
    1
  );
  Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT").add(
    "a",
    1
  );

  // Check that registeredHistogram works properly
  Telemetry.canRecordExtended = true;
  let registered = Telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  );
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(registered.has("TELEMETRY_TEST_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTIN"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTOUT"));
  Telemetry.canRecordExtended = false;
  registered = Telemetry.getSnapshotForHistograms("main", false /* clear */);
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(!registered.has("TELEMETRY_TEST_FLAG"));
  Assert.ok(!registered.has("TELEMETRY_TEST_RELEASE_OPTIN"));
  Assert.ok(registered.has("TELEMETRY_TEST_RELEASE_OPTOUT"));

  // Check that registeredKeyedHistograms works properly
  Telemetry.canRecordExtended = true;
  registered = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  );
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"));
  Telemetry.canRecordExtended = false;
  registered = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  );
  registered = new Set(Object.keys(registered.parent));
  Assert.ok(!registered.has("TELEMETRY_TEST_KEYED_FLAG"));
  Assert.ok(registered.has("TELEMETRY_TEST_KEYED_RELEASE_OPTOUT"));

  Telemetry.canRecordExtended = currentRecordExtended;
});

add_task(async function test_keyed_keys() {
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_KEYS");
  h.clear();
  Telemetry.clearScalars();

  // The |add| method should not throw for keys that are not allowed.
  h.add("testkey", true);
  h.add("thirdKey", false);
  h.add("not-allowed", true);

  // Check that we have the expected keys.
  let snap = h.snapshot();
  Assert.equal(Object.keys(snap).length, 2, "Only 2 keys must be recorded.");
  Assert.ok("testkey" in snap, "'testkey' must be recorded.");
  Assert.ok("thirdKey" in snap, "'thirdKey' must be recorded.");
  Assert.deepEqual(
    snap.testkey.values,
    { 0: 0, 1: 1, 2: 0 },
    "'testkey' must contain the correct value."
  );
  Assert.deepEqual(
    snap.thirdKey.values,
    { 0: 1, 1: 0 },
    "'thirdKey' must contain the correct value."
  );

  // Keys that are not allowed must not be recorded.
  Assert.ok(!("not-allowed" in snap), "'not-allowed' must not be recorded.");

  // Check that these failures were correctly tracked.
  const parentScalars = Telemetry.getSnapshotForKeyedScalars("main", false)
    .parent;
  const scalarName = "telemetry.accumulate_unknown_histogram_keys";
  Assert.ok(
    scalarName in parentScalars,
    "Accumulation to unallowed keys must be reported."
  );
  Assert.ok(
    "TELEMETRY_TEST_KEYED_KEYS" in parentScalars[scalarName],
    "Accumulation to unallowed keys must be recorded with the correct key."
  );
  Assert.equal(
    parentScalars[scalarName].TELEMETRY_TEST_KEYED_KEYS,
    1,
    "Accumulation to unallowed keys must report the correct value."
  );
});

add_task(async function test_count_multiple_samples() {
  let valid = [1, 1, 3, 0];
  let invalid = ["1", "0", "", "random"];

  let h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  h.clear();

  // If the array contains even a single invalid value, no accumulation should take place
  // Keep the valid values in front of invalid to check if it is simply accumulating as
  // it's traversing the array and throwing upon first invalid value. That should not happen.
  h.add(valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.equal(s1.sum, 0);
  // Ensure that no accumulations of 0-like values took place.
  // These accumulations won't increase the sum.
  Assert.deepEqual({}, s1.values);

  h.add(valid);
  let s2 = h.snapshot();
  Assert.deepEqual(s2.values, { 0: 4, 1: 0 });
  Assert.equal(s2.sum, 5);
});

add_task(async function test_categorical_multiple_samples() {
  let h = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL");
  h.clear();
  let valid = ["CommonLabel", "Label2", "Label3", "Label3", 0, 0, 1];
  let invalid = ["", "Label4", "1234", "0", "1", 5000];

  // At least one invalid parameter, so no accumulation should happen here
  // Valid values in front of invalid.
  h.add(valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.equal(s1.sum, 0);
  Assert.deepEqual({}, s1.values);

  h.add(valid);
  let snapshot = h.snapshot();
  Assert.equal(snapshot.sum, 6);
  Assert.deepEqual(snapshot.values, { 0: 3, 1: 2, 2: 2, 3: 0 });
});

add_task(async function test_boolean_multiple_samples() {
  let valid = [true, false, 0, 1, 2];
  let invalid = ["", "0", "1", ",2", "true", "false", "random"];

  let h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  h.clear();

  // At least one invalid parameter, so no accumulation should happen here
  // Valid values in front of invalid.
  h.add(valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.equal(s1.sum, 0);
  Assert.deepEqual({}, s1.values);

  h.add(valid);
  let s = h.snapshot();
  Assert.deepEqual(s.values, { 0: 2, 1: 3, 2: 0 });
  Assert.equal(s.sum, 3);
});

add_task(async function test_linear_multiple_samples() {
  // According to telemetry.mozilla.org/histogram-simulator, bucket at
  // index 1 of TELEMETRY_TEST_LINEAR has max value of 268.44M
  let valid = [0, 1, 5, 10, 268450000, 268450001, Math.pow(2, 31) + 1];
  let invalid = ["", "0", "1", "random"];

  let h = Telemetry.getHistogramById("TELEMETRY_TEST_LINEAR");
  h.clear();

  // At least one invalid paramater, so no accumulations.
  // Valid values in front of invalid.
  h.add(valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.equal(s1.sum, 0);
  Assert.deepEqual({}, s1.values);

  h.add(valid);
  let s2 = h.snapshot();
  // Values >= INT32_MAX are accumulated as INT32_MAX - 1
  Assert.equal(s2.sum, valid.reduce((acc, cur) => acc + cur) - 3);
  Assert.deepEqual(Object.values(s2.values), [1, 3, 2, 1]);
});

add_task(async function test_keyed_no_arguments() {
  // Test for no accumulation when add is called with no arguments
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_LINEAR");
  h.clear();

  h.add();

  // No keys should be added due to no accumulation.
  Assert.equal(h.keys().length, 0);
});

add_task(async function test_keyed_categorical_invalid_string() {
  // Test for no accumulation when add is called on a
  // keyed categorical histogram with an invalid string label.
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_CATEGORICAL");
  h.clear();

  h.add("someKey", "#notALabel");

  // No keys should be added due to no accumulation.
  Assert.equal(h.keys().length, 0);
});

add_task(async function test_keyed_count_multiple_samples() {
  let valid = [1, 1, 3, 0];
  let invalid = ["1", "0", "", "random"];
  let key = "somekeystring";

  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  h.clear();

  // If the array contains even a single invalid value, no accumulation should take place
  // Keep the valid values in front of invalid to check if it is simply accumulating as
  // it's traversing the array and throwing upon first invalid value. That should not happen.
  h.add(key, valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.ok(!(key in s1));

  h.add(key, valid);
  let s2 = h.snapshot()[key];
  Assert.deepEqual(s2.values, { 0: 4, 1: 0 });
  Assert.equal(s2.sum, 5);
});

add_task(async function test_keyed_categorical_multiple_samples() {
  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_CATEGORICAL");
  h.clear();
  let valid = ["CommonLabel", "Label2", "Label3", "Label3", 0, 0, 1];
  let invalid = ["", "Label4", "1234", "0", "1", 5000];
  let key = "somekeystring";

  // At least one invalid parameter, so no accumulation should happen here
  // Valid values in front of invalid.
  h.add(key, valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.ok(!(key in s1));

  h.add(key, valid);
  let snapshot = h.snapshot()[key];
  Assert.equal(snapshot.sum, 6);
  Assert.deepEqual(Object.values(snapshot.values), [3, 2, 2, 0]);
});

add_task(async function test_keyed_boolean_multiple_samples() {
  let valid = [true, false, 0, 1, 2];
  let invalid = ["", "0", "1", ",2", "true", "false", "random"];
  let key = "somekey";

  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_BOOLEAN");
  h.clear();

  // At least one invalid parameter, so no accumulation should happen here
  // Valid values in front of invalid.
  h.add(key, valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.ok(!(key in s1));

  h.add(key, valid);
  let s = h.snapshot()[key];
  Assert.deepEqual(s.values, { 0: 2, 1: 3, 2: 0 });
  Assert.equal(s.sum, 3);
});

add_task(async function test_keyed_linear_multiple_samples() {
  // According to telemetry.mozilla.org/histogram-simulator, bucket at
  // index 1 of TELEMETRY_TEST_LINEAR has max value of 3.13K
  let valid = [0, 1, 5, 10, 268450000, 268450001, Math.pow(2, 31) + 1];
  let invalid = ["", "0", "1", "random"];
  let key = "somestring";

  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_LINEAR");
  h.clear();

  // At least one invalid paramater, so no accumulations.
  // Valid values in front of invalid.
  h.add(key, valid.concat(invalid));
  let s1 = h.snapshot();
  Assert.ok(!(key in s1));

  h.add(key, valid);
  let s2 = h.snapshot()[key];
  // Values >= INT32_MAX are accumulated as INT32_MAX - 1
  Assert.equal(s2.sum, valid.reduce((acc, cur) => acc + cur) - 3);
  Assert.deepEqual(s2.range, [1, 250000]);
  Assert.deepEqual(s2.values, { 0: 1, 1: 3, 250000: 3 });
});

add_task(async function test_non_array_non_string_obj() {
  let invalid_obj = {
    prop1: "someValue",
    prop2: "someOtherValue",
  };
  let key = "someString";

  let h = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_LINEAR");
  h.clear();

  h.add(key, invalid_obj);
  Assert.equal(h.keys().length, 0);
});

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_productSpecificHistograms() {
    const DEFAULT_PRODUCTS_HISTOGRAM = "TELEMETRY_TEST_DEFAULT_PRODUCTS";
    const DESKTOP_ONLY_HISTOGRAM = "TELEMETRY_TEST_DESKTOP_ONLY";
    const MULTIPRODUCT_HISTOGRAM = "TELEMETRY_TEST_MULTIPRODUCT";
    const MOBILE_ONLY_HISTOGRAM = "TELEMETRY_TEST_MOBILE_ONLY";

    var default_histo = Telemetry.getHistogramById(DEFAULT_PRODUCTS_HISTOGRAM);
    var desktop_histo = Telemetry.getHistogramById(DESKTOP_ONLY_HISTOGRAM);
    var multiproduct_histo = Telemetry.getHistogramById(MULTIPRODUCT_HISTOGRAM);
    var mobile_histo = Telemetry.getHistogramById(MOBILE_ONLY_HISTOGRAM);
    default_histo.clear();
    desktop_histo.clear();
    multiproduct_histo.clear();
    mobile_histo.clear();

    default_histo.add(42);
    desktop_histo.add(42);
    multiproduct_histo.add(42);
    mobile_histo.add(42);

    let histograms = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;

    Assert.ok(
      DEFAULT_PRODUCTS_HISTOGRAM in histograms,
      "Should have recorded default products histogram"
    );
    Assert.ok(
      DESKTOP_ONLY_HISTOGRAM in histograms,
      "Should have recorded desktop-only histogram"
    );
    Assert.ok(
      MULTIPRODUCT_HISTOGRAM in histograms,
      "Should have recorded multiproduct histogram"
    );

    Assert.ok(
      !(MOBILE_ONLY_HISTOGRAM in histograms),
      "Should not have recorded mobile-only histogram"
    );
  }
);

add_task(
  {
    skip_if: () => !gIsAndroid,
  },
  async function test_mobileSpecificHistograms() {
    const DEFAULT_PRODUCTS_HISTOGRAM = "TELEMETRY_TEST_DEFAULT_PRODUCTS";
    const DESKTOP_ONLY_HISTOGRAM = "TELEMETRY_TEST_DESKTOP_ONLY";
    const MULTIPRODUCT_HISTOGRAM = "TELEMETRY_TEST_MULTIPRODUCT";
    const MOBILE_ONLY_HISTOGRAM = "TELEMETRY_TEST_MOBILE_ONLY";

    var default_histo = Telemetry.getHistogramById(DEFAULT_PRODUCTS_HISTOGRAM);
    var desktop_histo = Telemetry.getHistogramById(DESKTOP_ONLY_HISTOGRAM);
    var multiproduct_histo = Telemetry.getHistogramById(MULTIPRODUCT_HISTOGRAM);
    var mobile_histo = Telemetry.getHistogramById(MOBILE_ONLY_HISTOGRAM);
    default_histo.clear();
    desktop_histo.clear();
    multiproduct_histo.clear();
    mobile_histo.clear();

    default_histo.add(1);
    desktop_histo.add(1);
    multiproduct_histo.add(1);
    mobile_histo.add(1);

    let histograms = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;

    Assert.ok(
      DEFAULT_PRODUCTS_HISTOGRAM in histograms,
      "Should have recorded default products histogram"
    );
    Assert.ok(
      MOBILE_ONLY_HISTOGRAM in histograms,
      "Should have recorded mobile-only histogram"
    );
    Assert.ok(
      MULTIPRODUCT_HISTOGRAM in histograms,
      "Should have recorded multiproduct histogram"
    );

    Assert.ok(
      !(DESKTOP_ONLY_HISTOGRAM in histograms),
      "Should not have recorded desktop-only histogram"
    );
  }
);

add_task(async function test_productsOverride() {
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );
  const DEFAULT_PRODUCTS_HISTOGRAM = "TELEMETRY_TEST_DEFAULT_PRODUCTS";
  const DESKTOP_ONLY_HISTOGRAM = "TELEMETRY_TEST_DESKTOP_ONLY";
  const MULTIPRODUCT_HISTOGRAM = "TELEMETRY_TEST_MULTIPRODUCT";
  const MOBILE_ONLY_HISTOGRAM = "TELEMETRY_TEST_MOBILE_ONLY";

  var default_histo = Telemetry.getHistogramById(DEFAULT_PRODUCTS_HISTOGRAM);
  var desktop_histo = Telemetry.getHistogramById(DESKTOP_ONLY_HISTOGRAM);
  var multiproduct_histo = Telemetry.getHistogramById(MULTIPRODUCT_HISTOGRAM);
  var mobile_histo = Telemetry.getHistogramById(MOBILE_ONLY_HISTOGRAM);
  default_histo.clear();
  desktop_histo.clear();
  multiproduct_histo.clear();
  mobile_histo.clear();

  default_histo.add(1);
  desktop_histo.add(1);
  multiproduct_histo.add(1);
  mobile_histo.add(1);

  let histograms = Telemetry.getSnapshotForHistograms("main", false /* clear */)
    .parent;

  Assert.ok(
    DEFAULT_PRODUCTS_HISTOGRAM in histograms,
    "Should have recorded default products histogram"
  );
  Assert.ok(
    MOBILE_ONLY_HISTOGRAM in histograms,
    "Should have recorded mobile-only histogram"
  );
  Assert.ok(
    MULTIPRODUCT_HISTOGRAM in histograms,
    "Should have recorded multiproduct histogram"
  );

  Assert.ok(
    DESKTOP_ONLY_HISTOGRAM in histograms,
    "Should not have recorded desktop-only histogram"
  );
  Services.prefs.clearUserPref(
    "toolkit.telemetry.testing.overrideProductsCheck"
  );
});

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_clearHistogramsOnSnapshot() {
    const COUNT = "TELEMETRY_TEST_COUNT";
    let h = Telemetry.getHistogramById(COUNT);
    h.clear();
    let snapshot;

    // The first snapshot should be empty, nothing recorded.
    snapshot = Telemetry.getSnapshotForHistograms("main", false /* clear */)
      .parent;
    Assert.ok(!(COUNT in snapshot));

    // After recording into a histogram, the data should be in the snapshot. Don't delete it.
    h.add(1);

    Assert.equal(h.snapshot().sum, 1);
    snapshot = Telemetry.getSnapshotForHistograms("main", false /* clear */)
      .parent;
    Assert.ok(COUNT in snapshot);
    Assert.equal(snapshot[COUNT].sum, 1);

    // After recording into a histogram again, the data should be updated and in the snapshot.
    // Clean up after.
    h.add(41);

    Assert.equal(h.snapshot().sum, 42);
    snapshot = Telemetry.getSnapshotForHistograms("main", true /* clear */)
      .parent;
    Assert.ok(COUNT in snapshot);
    Assert.equal(snapshot[COUNT].sum, 42);

    // Finally, no data should be in the snapshot.
    Assert.equal(h.snapshot().sum, 0);
    snapshot = Telemetry.getSnapshotForHistograms("main", false /* clear */)
      .parent;
    Assert.ok(!(COUNT in snapshot));
  }
);

add_task(async function test_valid_os_smoketest() {
  let nonExistingProbe;
  let existingProbe;

  switch (AppConstants.platform) {
    case "linux":
      nonExistingProbe = "TELEMETRY_TEST_OS_ANDROID_ONLY";
      existingProbe = "TELEMETRY_TEST_OS_LINUX_ONLY";
      break;
    case "macosx":
      nonExistingProbe = "TELEMETRY_TEST_OS_ANDROID_ONLY";
      existingProbe = "TELEMETRY_TEST_OS_MAC_ONLY";
      break;
    case "win":
      nonExistingProbe = "TELEMETRY_TEST_OS_ANDROID_ONLY";
      existingProbe = "TELEMETRY_TEST_OS_WIN_ONLY";
      break;
    case "android":
      nonExistingProbe = "TELEMETRY_TEST_OS_LINUX_ONLY";
      existingProbe = "TELEMETRY_TEST_OS_ANDROID_ONLY";
      break;
    default:
      /* Unknown OS. Let's not test OS-specific probes */
      return;
  }

  Assert.throws(
    () => Telemetry.getHistogramById(nonExistingProbe),
    /NS_ERROR_FAILURE/,
    `Should throw on ${nonExistingProbe} probe that's not available on ${AppConstants.platform}`
  );

  let h = Telemetry.getHistogramById(existingProbe);
  h.clear();
  h.add(1);
  let snapshot = Telemetry.getSnapshotForHistograms("main", false /* clear */)
    .parent;
  Assert.ok(
    existingProbe in snapshot,
    `${existingProbe} should be recorded on ${AppConstants.platform}`
  );
  Assert.equal(snapshot[existingProbe].sum, 1);
});

add_task(async function test_multistore_individual_histogram() {
  Telemetry.canRecordExtended = true;

  let id;
  let hist;
  let snapshot;

  id = "TELEMETRY_TEST_MAIN_ONLY";
  hist = Telemetry.getHistogramById(id);
  snapshot = hist.snapshot();
  Assert.equal(0, snapshot.sum, `Histogram ${id} should be empty.`);
  hist.add(1);
  snapshot = hist.snapshot();
  Assert.equal(
    1,
    snapshot.sum,
    `Histogram ${id} should have recorded one value.`
  );
  hist.clear();
  snapshot = hist.snapshot();
  Assert.equal(0, snapshot.sum, `Histogram ${id} should be cleared.`);

  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  hist = Telemetry.getHistogramById(id);
  snapshot = hist.snapshot();
  Assert.equal(0, snapshot.sum, `Histogram ${id} should be empty.`);
  hist.add(1);
  snapshot = hist.snapshot();
  Assert.equal(
    1,
    snapshot.sum,
    `Histogram ${id} should have recorded one value.`
  );
  hist.clear();
  snapshot = hist.snapshot();
  Assert.equal(0, snapshot.sum, `Histogram ${id} should be cleared.`);

  // When sync only, then the snapshot will be empty on the main store
  id = "TELEMETRY_TEST_SYNC_ONLY";
  hist = Telemetry.getHistogramById(id);
  snapshot = hist.snapshot();
  Assert.equal(
    undefined,
    snapshot,
    `Histogram ${id} should not be in the 'main' storage`
  );
  hist.add(1);
  snapshot = hist.snapshot();
  Assert.equal(
    undefined,
    snapshot,
    `Histogram ${id} should not be in the 'main' storage`
  );
  hist.clear();
  snapshot = hist.snapshot();
  Assert.equal(
    undefined,
    snapshot,
    `Histogram ${id} should not be in the 'main' storage`
  );

  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  hist = Telemetry.getKeyedHistogramById(id);
  snapshot = hist.snapshot();
  Assert.deepEqual({}, snapshot, `Histogram ${id} should be empty.`);
  hist.add("key-a", 1);
  snapshot = hist.snapshot();
  Assert.equal(
    1,
    snapshot["key-a"].sum,
    `Histogram ${id} should have recorded one value.`
  );
  hist.clear();
  snapshot = hist.snapshot();
  Assert.deepEqual({}, snapshot, `Histogram ${id} should be cleared.`);

  // When sync only, then the snapshot will be empty on the main store
  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  hist = Telemetry.getKeyedHistogramById(id);
  snapshot = hist.snapshot();
  Assert.equal(
    undefined,
    snapshot,
    `Histogram ${id} should not be in the 'main' storage`
  );
  hist.add("key-a", 1);
  snapshot = hist.snapshot();
  Assert.equal(
    undefined,
    snapshot,
    `Histogram ${id} should not be in the 'main' storage`
  );
  hist.clear();
  snapshot = hist.snapshot();
  Assert.equal(
    undefined,
    snapshot,
    `Histogram ${id} should not be in the 'main' storage`
  );
});

add_task(async function test_multistore_main_snapshot() {
  Telemetry.canRecordExtended = true;
  // Clear histograms
  Telemetry.getSnapshotForHistograms("main", true);
  Telemetry.getSnapshotForKeyedHistograms("main", true);

  let id;
  let hist;
  let snapshot;

  // Plain histograms

  // Fill with data
  id = "TELEMETRY_TEST_MAIN_ONLY";
  hist = Telemetry.getHistogramById(id);
  hist.add(1);

  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  hist = Telemetry.getHistogramById(id);
  hist.add(1);

  id = "TELEMETRY_TEST_SYNC_ONLY";
  hist = Telemetry.getHistogramById(id);
  hist.add(1);

  // Getting snapshot and NOT clearing (using default values for optional parameters)
  snapshot = Telemetry.getSnapshotForHistograms().parent;
  id = "TELEMETRY_TEST_MAIN_ONLY";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  // Data should still be in, getting snapshot and clearing
  snapshot = Telemetry.getSnapshotForHistograms("main", /* clear */ true)
    .parent;
  id = "TELEMETRY_TEST_MAIN_ONLY";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  // Should be empty after clearing
  snapshot = Telemetry.getSnapshotForHistograms("main", /* clear */ false)
    .parent;
  id = "TELEMETRY_TEST_MAIN_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);
  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);
  id = "TELEMETRY_TEST_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  // Keyed histograms

  // Fill with data
  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  hist = Telemetry.getKeyedHistogramById(id);
  hist.add("key-a", 1);

  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  hist = Telemetry.getKeyedHistogramById(id);
  hist.add("key-b", 1);

  // Getting snapshot and NOT clearing (using default values for optional parameters)
  snapshot = Telemetry.getSnapshotForKeyedHistograms().parent;
  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  // Data should still be in, getting snapshot and clearing
  snapshot = Telemetry.getSnapshotForKeyedHistograms("main", /* clear */ true)
    .parent;
  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  // Should be empty after clearing
  snapshot = Telemetry.getSnapshotForKeyedHistograms("main", /* clear */ false)
    .parent;
  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);
  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);
});

add_task(async function test_multistore_argument_handling() {
  Telemetry.canRecordExtended = true;
  // Clear histograms
  Telemetry.getSnapshotForHistograms("main", true);
  Telemetry.getSnapshotForHistograms("sync", true);
  Telemetry.getSnapshotForKeyedHistograms("main", true);
  Telemetry.getSnapshotForKeyedHistograms("sync", true);

  let id;
  let hist;
  let snapshot;

  // Plain Histograms

  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  hist = Telemetry.getHistogramById(id);
  hist.add(37);

  // No argument
  snapshot = hist.snapshot();
  Assert.equal(37, snapshot.sum, `${id} should be in a default store snapshot`);

  hist.clear();
  snapshot = hist.snapshot();
  Assert.equal(0, snapshot.sum, `${id} should be cleared in the default store`);

  snapshot = hist.snapshot({ store: "sync" });
  Assert.equal(
    37,
    snapshot.sum,
    `${id} should not have been cleared in the sync store`
  );

  Assert.throws(
    () => hist.snapshot(2, "or", "more", "arguments"),
    /one argument/,
    "snapshot should check argument count"
  );
  Assert.throws(
    () => hist.snapshot(2),
    /object argument/,
    "snapshot should check argument type"
  );
  Assert.throws(
    () => hist.snapshot({}),
    /property/,
    "snapshot should check for object property"
  );
  Assert.throws(
    () => hist.snapshot({ store: 1 }),
    /string/,
    "snapshot should check object property's type"
  );

  Assert.throws(
    () => hist.clear(2, "or", "more", "arguments"),
    /one argument/,
    "clear should check argument count"
  );
  Assert.throws(
    () => hist.clear(2),
    /object argument/,
    "clear should check argument type"
  );
  Assert.throws(
    () => hist.clear({}),
    /property/,
    "clear should check for object property"
  );
  Assert.throws(
    () => hist.clear({ store: 1 }),
    /string/,
    "clear should check object property's type"
  );

  // Keyed Histogram

  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  hist = Telemetry.getKeyedHistogramById(id);
  hist.add("key-1", 37);

  // No argument
  snapshot = hist.snapshot();
  Assert.equal(
    37,
    snapshot["key-1"].sum,
    `${id} should be in a default store snapshot`
  );

  hist.clear();
  snapshot = hist.snapshot();
  Assert.ok(
    !("key-1" in snapshot),
    `${id} should be cleared in the default store`
  );

  snapshot = hist.snapshot({ store: "sync" });
  Assert.equal(
    37,
    snapshot["key-1"].sum,
    `${id} should not have been cleared in the sync store`
  );

  Assert.throws(
    () => hist.snapshot(2, "or", "more", "arguments"),
    /one argument/,
    "snapshot should check argument count"
  );
  Assert.throws(
    () => hist.snapshot(2),
    /object argument/,
    "snapshot should check argument type"
  );
  Assert.throws(
    () => hist.snapshot({}),
    /property/,
    "snapshot should check for object property"
  );
  Assert.throws(
    () => hist.snapshot({ store: 1 }),
    /string/,
    "snapshot should check object property's type"
  );

  Assert.throws(
    () => hist.clear(2, "or", "more", "arguments"),
    /one argument/,
    "clear should check argument count"
  );
  Assert.throws(
    () => hist.clear(2),
    /object argument/,
    "clear should check argument type"
  );
  Assert.throws(
    () => hist.clear({}),
    /property/,
    "clear should check for object property"
  );
  Assert.throws(
    () => hist.clear({ store: 1 }),
    /string/,
    "clear should check object property's type"
  );
});

add_task(async function test_multistore_sync_snapshot() {
  Telemetry.canRecordExtended = true;
  // Clear histograms
  Telemetry.getSnapshotForHistograms("main", true);
  Telemetry.getSnapshotForHistograms("sync", true);

  let id;
  let hist;
  let snapshot;

  // Plain histograms

  // Fill with data
  id = "TELEMETRY_TEST_MAIN_ONLY";
  hist = Telemetry.getHistogramById(id);
  hist.add(1);

  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  hist = Telemetry.getHistogramById(id);
  hist.add(1);

  id = "TELEMETRY_TEST_SYNC_ONLY";
  hist = Telemetry.getHistogramById(id);
  hist.add(1);

  // Getting snapshot and clearing
  snapshot = Telemetry.getSnapshotForHistograms("main", /* clear */ true)
    .parent;
  id = "TELEMETRY_TEST_MAIN_ONLY";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  snapshot = Telemetry.getSnapshotForHistograms("sync", /* clear */ true)
    .parent;
  id = "TELEMETRY_TEST_MAIN_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a sync store snapshot`);
  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a sync store snapshot`);
  id = "TELEMETRY_TEST_SYNC_ONLY";
  Assert.ok(id in snapshot, `${id} should be in a sync store snapshot`);
});

add_task(async function test_multistore_keyed_sync_snapshot() {
  Telemetry.canRecordExtended = true;
  // Clear histograms
  Telemetry.getSnapshotForKeyedHistograms("main", true);
  Telemetry.getSnapshotForKeyedHistograms("sync", true);

  let id;
  let hist;
  let snapshot;

  // Plain histograms

  // Fill with data
  id = "TELEMETRY_TEST_KEYED_LINEAR";
  hist = Telemetry.getKeyedHistogramById(id);
  hist.add("key-1", 1);

  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  hist = Telemetry.getKeyedHistogramById(id);
  hist.add("key-1", 1);

  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  hist = Telemetry.getKeyedHistogramById(id);
  hist.add("key-1", 1);

  // Getting snapshot and clearing
  snapshot = Telemetry.getSnapshotForKeyedHistograms("main", /* clear */ true)
    .parent;
  id = "TELEMETRY_TEST_KEYED_LINEAR";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a main store snapshot`);
  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  Assert.ok(!(id in snapshot), `${id} should not be in a main store snapshot`);

  snapshot = Telemetry.getSnapshotForKeyedHistograms("sync", /* clear */ true)
    .parent;
  id = "TELEMETRY_TEST_KEYED_LINEAR";
  Assert.ok(!(id in snapshot), `${id} should not be in a sync store snapshot`);
  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  Assert.ok(id in snapshot, `${id} should be in a sync store snapshot`);
  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  Assert.ok(id in snapshot, `${id} should be in a sync store snapshot`);
});

add_task(async function test_multistore_plain_individual_snapshot() {
  Telemetry.canRecordExtended = true;
  // Clear histograms
  Telemetry.getSnapshotForHistograms("main", true);
  Telemetry.getSnapshotForHistograms("sync", true);

  let id;
  let hist;

  id = "TELEMETRY_TEST_MAIN_ONLY";
  hist = Telemetry.getHistogramById(id);

  hist.add(37);
  Assert.deepEqual(37, hist.snapshot({ store: "main" }).sum);
  Assert.deepEqual(undefined, hist.snapshot({ store: "sync" }));

  hist.clear({ store: "main" });
  Assert.deepEqual(0, hist.snapshot({ store: "main" }).sum);
  Assert.deepEqual(undefined, hist.snapshot({ store: "sync" }));

  id = "TELEMETRY_TEST_MULTIPLE_STORES";
  hist = Telemetry.getHistogramById(id);

  hist.add(37);
  Assert.deepEqual(37, hist.snapshot({ store: "main" }).sum);
  Assert.deepEqual(37, hist.snapshot({ store: "sync" }).sum);

  hist.clear({ store: "main" });
  Assert.deepEqual(0, hist.snapshot({ store: "main" }).sum);
  Assert.deepEqual(37, hist.snapshot({ store: "sync" }).sum);

  hist.add(3);
  Assert.deepEqual(3, hist.snapshot({ store: "main" }).sum);
  Assert.deepEqual(40, hist.snapshot({ store: "sync" }).sum);

  hist.clear({ store: "sync" });
  Assert.deepEqual(3, hist.snapshot({ store: "main" }).sum);
  Assert.deepEqual(0, hist.snapshot({ store: "sync" }).sum);

  id = "TELEMETRY_TEST_SYNC_ONLY";
  hist = Telemetry.getHistogramById(id);

  hist.add(37);
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(37, hist.snapshot({ store: "sync" }).sum);

  hist.clear({ store: "main" });
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(37, hist.snapshot({ store: "sync" }).sum);

  hist.add(3);
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(40, hist.snapshot({ store: "sync" }).sum);

  hist.clear({ store: "sync" });
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(0, hist.snapshot({ store: "sync" }).sum);
});

add_task(async function test_multistore_keyed_individual_snapshot() {
  Telemetry.canRecordExtended = true;
  // Clear histograms
  Telemetry.getSnapshotForKeyedHistograms("main", true);
  Telemetry.getSnapshotForKeyedHistograms("sync", true);

  let id;
  let hist;

  id = "TELEMETRY_TEST_KEYED_LINEAR";
  hist = Telemetry.getKeyedHistogramById(id);

  hist.add("key-1", 37);
  Assert.deepEqual(37, hist.snapshot({ store: "main" })["key-1"].sum);
  Assert.deepEqual(undefined, hist.snapshot({ store: "sync" }));

  hist.clear({ store: "main" });
  Assert.deepEqual({}, hist.snapshot({ store: "main" }));
  Assert.deepEqual(undefined, hist.snapshot({ store: "sync" }));

  hist.add("key-1", 4);
  hist.clear({ store: "sync" });
  Assert.deepEqual(4, hist.snapshot({ store: "main" })["key-1"].sum);
  Assert.deepEqual(undefined, hist.snapshot({ store: "sync" }));

  id = "TELEMETRY_TEST_KEYED_MULTIPLE_STORES";
  hist = Telemetry.getKeyedHistogramById(id);

  hist.add("key-1", 37);
  Assert.deepEqual(37, hist.snapshot({ store: "main" })["key-1"].sum);
  Assert.deepEqual(37, hist.snapshot({ store: "sync" })["key-1"].sum);

  hist.clear({ store: "main" });
  Assert.deepEqual({}, hist.snapshot({ store: "main" }));
  Assert.deepEqual(37, hist.snapshot({ store: "sync" })["key-1"].sum);

  hist.add("key-1", 3);
  Assert.deepEqual(3, hist.snapshot({ store: "main" })["key-1"].sum);
  Assert.deepEqual(40, hist.snapshot({ store: "sync" })["key-1"].sum);

  hist.clear({ store: "sync" });
  Assert.deepEqual(3, hist.snapshot({ store: "main" })["key-1"].sum);
  Assert.deepEqual({}, hist.snapshot({ store: "sync" }));

  id = "TELEMETRY_TEST_KEYED_SYNC_ONLY";
  hist = Telemetry.getKeyedHistogramById(id);

  hist.add("key-1", 37);
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(37, hist.snapshot({ store: "sync" })["key-1"].sum);

  hist.clear({ store: "main" });
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(37, hist.snapshot({ store: "sync" })["key-1"].sum);

  hist.add("key-1", 3);
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual(40, hist.snapshot({ store: "sync" })["key-1"].sum);

  hist.clear({ store: "sync" });
  Assert.deepEqual(undefined, hist.snapshot({ store: "main" }));
  Assert.deepEqual({}, hist.snapshot({ store: "sync" }));
});

add_task(async function test_can_record_in_process_regression_bug_1530361() {
  Telemetry.getSnapshotForHistograms("main", true);

  // The socket and gpu processes should not have any histograms.
  // Flag and count histograms have defaults, so if we're accidentally recording them
  // in these processes they'd show up even immediately after being cleared.
  let snapshot = Telemetry.getSnapshotForHistograms("main", true);

  Assert.deepEqual(
    snapshot.gpu,
    {},
    "No histograms should have been recorded for the gpu process"
  );
  Assert.deepEqual(
    snapshot.socket,
    {},
    "No histograms should have been recorded for the socket process"
  );
});

add_task(function test_knows_its_name() {
  let h;

  // Plain histograms
  const histNames = [
    "TELEMETRY_TEST_FLAG",
    "TELEMETRY_TEST_COUNT",
    "TELEMETRY_TEST_CATEGORICAL",
    "TELEMETRY_TEST_EXPIRED",
  ];

  for (let name of histNames) {
    h = Telemetry.getHistogramById(name);
    Assert.equal(name, h.name());
  }

  // Keyed histograms
  const keyedHistNames = [
    "TELEMETRY_TEST_KEYED_EXPONENTIAL",
    "TELEMETRY_TEST_KEYED_BOOLEAN",
    "TELEMETRY_TEST_EXPIRED_KEYED",
  ];

  for (let name of keyedHistNames) {
    h = Telemetry.getKeyedHistogramById(name);
    Assert.equal(name, h.name());
  }
});
