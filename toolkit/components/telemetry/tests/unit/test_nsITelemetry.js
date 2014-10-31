/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const INT_MAX = 0x7FFFFFFF;

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
Cu.import("resource://gre/modules/Services.jsm", this);

function test_expired_histogram() {
  var histogram_id = "FOOBAR";
  var test_expired_id = "TELEMETRY_TEST_EXPIRED";
  var clone_id = "ExpiredClone";
  var dummy = Telemetry.newHistogram(histogram_id, "28.0a1", Telemetry.HISTOGRAM_EXPONENTIAL, 1, 2, 3);
  var dummy_clone = Telemetry.histogramFrom(clone_id, test_expired_id);
  var rh = Telemetry.registeredHistograms([]);

  dummy.add(1);
  dummy_clone.add(1);

  do_check_eq(Telemetry.histogramSnapshots["__expired__"], undefined);
  do_check_eq(Telemetry.histogramSnapshots[histogram_id], undefined);
  do_check_eq(Telemetry.histogramSnapshots[test_expired_id], undefined);
  do_check_eq(Telemetry.histogramSnapshots[clone_id], undefined);
  do_check_eq(rh[test_expired_id], undefined);
}

function test_histogram(histogram_type, name, min, max, bucket_count) {
  var h = Telemetry.newHistogram(name, "never", histogram_type, min, max, bucket_count);
  var r = h.snapshot().ranges;
  var sum = 0;
  var log_sum = 0;
  var log_sum_squares = 0;
  for(var i=0;i<r.length;i++) {
    var v = r[i];
    sum += v;
    if (histogram_type == Telemetry.HISTOGRAM_EXPONENTIAL) {
      var log_v = Math.log(1+v);
      log_sum += log_v;
      log_sum_squares += log_v*log_v;
    }
    h.add(v);
  }
  var s = h.snapshot();
  // verify properties
  do_check_eq(sum, s.sum);
  if (histogram_type == Telemetry.HISTOGRAM_EXPONENTIAL) {
    // We do the log with float precision in C++ and double precision in
    // JS, so there's bound to be tiny discrepancies.  Just check the
    // integer part.
    do_check_eq(Math.floor(log_sum), Math.floor(s.log_sum));
    do_check_eq(Math.floor(log_sum_squares), Math.floor(s.log_sum_squares));
    do_check_false("sum_squares_lo" in s);
    do_check_false("sum_squares_hi" in s);
  } else {
    // Doing the math to verify sum_squares was reflected correctly is
    // tedious in JavaScript.  Just make sure we have something.
    do_check_neq(s.sum_squares_lo + s.sum_squares_hi, 0);
    do_check_false("log_sum" in s);
    do_check_false("log_sum_squares" in s);
  }

  // there should be exactly one element per bucket
  for each(var i in s.counts) {
    do_check_eq(i, 1);
  }
  var hgrams = Telemetry.histogramSnapshots
  gh = hgrams[name]
  do_check_eq(gh.histogram_type, histogram_type);

  do_check_eq(gh.min, min)
  do_check_eq(gh.max, max)

  // Check that booleans work with nonboolean histograms
  h.add(false);
  h.add(true);
  var s = h.snapshot().counts;
  do_check_eq(s[0], 2)
  do_check_eq(s[1], 2)

  // Check that clearing works.
  h.clear();
  var s = h.snapshot();
  for each(var i in s.counts) {
    do_check_eq(i, 0);
  }
  do_check_eq(s.sum, 0);
  if (histogram_type == Telemetry.HISTOGRAM_EXPONENTIAL) {
    do_check_eq(s.log_sum, 0);
    do_check_eq(s.log_sum_squares, 0);
  } else {
    do_check_eq(s.sum_squares_lo, 0);
    do_check_eq(s.sum_squares_hi, 0);
  }

  h.add(0);
  h.add(1);
  var c = h.snapshot().counts;
  do_check_eq(c[0], 1);
  do_check_eq(c[1], 1);
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

function test_boolean_histogram()
{
  var h = Telemetry.newHistogram("test::boolean histogram", "never", Telemetry.HISTOGRAM_BOOLEAN);
  var r = h.snapshot().ranges;
  // boolean histograms ignore numeric parameters
  do_check_eq(uneval(r), uneval([0, 1, 2]))
  var sum = 0
  for(var i=0;i<r.length;i++) {
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
}

function test_flag_histogram()
{
  var h = Telemetry.newHistogram("test::flag histogram", "never", Telemetry.HISTOGRAM_FLAG);
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
}

function test_count_histogram()
{
  let h = Telemetry.newHistogram("test::count histogram", "never", Telemetry.HISTOGRAM_COUNT, 1, 2, 3);
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
}

function test_getHistogramById() {
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
}

function compareHistograms(h1, h2) {
  let s1 = h1.snapshot();
  let s2 = h2.snapshot();

  do_check_eq(s1.histogram_type, s2.histogram_type);
  do_check_eq(s1.min, s2.min);
  do_check_eq(s1.max, s2.max);
  do_check_eq(s1.sum, s2.sum);
  if (s1.histogram_type == Telemetry.HISTOGRAM_EXPONENTIAL) {
    do_check_eq(s1.log_sum, s2.log_sum);
    do_check_eq(s1.log_sum_squares, s2.log_sum_squares);
  } else {
    do_check_eq(s1.sum_squares_lo, s2.sum_squares_lo);
    do_check_eq(s1.sum_squares_hi, s2.sum_squares_hi);
  }

  do_check_eq(s1.counts.length, s2.counts.length);
  for (let i = 0; i < s1.counts.length; i++)
    do_check_eq(s1.counts[i], s2.counts[i]);

  do_check_eq(s1.ranges.length, s2.ranges.length);
  for (let i = 0; i < s1.ranges.length; i++)
    do_check_eq(s1.ranges[i], s2.ranges[i]);
}

function test_histogramFrom() {
  // Test one histogram of each type.
  let names = [
      "CYCLE_COLLECTOR",      // EXPONENTIAL
      "GC_REASON_2",          // LINEAR
      "GC_RESET",             // BOOLEAN
      "TELEMETRY_TEST_FLAG",  // FLAG
      "TELEMETRY_TEST_COUNT", // COUNT
  ];

  for each (let name in names) {
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10]
    let original = Telemetry.getHistogramById(name);
    let clone = Telemetry.histogramFrom("clone" + name, name);
    compareHistograms(original, clone);
  }

  // Additionally, set TELEMETRY_TEST_FLAG and TELEMETRY_TEST_COUNT
  // and check they get set on the clone.
  let testFlag = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  testFlag.add(1);
  let testCount = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  testCount.add();
  let clone = Telemetry.histogramFrom("FlagClone", "TELEMETRY_TEST_FLAG");
  compareHistograms(testFlag, clone);
  clone = Telemetry.histogramFrom("CountClone", "TELEMETRY_TEST_COUNT");
  compareHistograms(testCount, clone);
}

function test_getSlowSQL() {
  var slow = Telemetry.slowSQL;
  do_check_true(("mainThread" in slow) && ("otherThreads" in slow));
}

function test_addons() {
  var addon_id = "testing-addon";
  var fake_addon_id = "fake-addon";
  var name1 = "testing-histogram1";
  var register = Telemetry.registerAddonHistogram;
  expect_success(function ()
                 register(addon_id, name1, 1, 5, 6, Telemetry.HISTOGRAM_LINEAR));
  // Can't register the same histogram multiple times.
  expect_fail(function ()
	      register(addon_id, name1, 1, 5, 6, Telemetry.HISTOGRAM_LINEAR));
  // Make sure we can't get at it with another name.
  expect_fail(function () Telemetry.getAddonHistogram(fake_addon_id, name1));

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
  expect_success(function ()
                 register(addon_id, name2, 2, 4, 4, Telemetry.HISTOGRAM_LINEAR));

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
  expect_success(function ()
		 register(extra_addon, name1, 0, 1, 2, Telemetry.HISTOGRAM_BOOLEAN));

  // Check that we can register flag histograms.
  var flag_addon = "testing-flag-addon";
  var flag_histogram = "flag-histogram";
  expect_success(function() 
                 register(flag_addon, flag_histogram, 0, 1, 2, Telemetry.HISTOGRAM_FLAG))
  expect_success(function()
		 register(flag_addon, name2, 2, 4, 4, Telemetry.HISTOGRAM_LINEAR));

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
}

// Check that telemetry doesn't record in private mode
function test_privateMode() {
  var h = Telemetry.newHistogram("test::private_mode_boolean", "never", Telemetry.HISTOGRAM_BOOLEAN);
  var orig = h.snapshot();
  Telemetry.canRecord = false;
  h.add(1);
  do_check_eq(uneval(orig), uneval(h.snapshot()));
  Telemetry.canRecord = true;
  h.add(1);
  do_check_neq(uneval(orig), uneval(h.snapshot()));
}

// Check that histograms that aren't flagged as needing extended stats
// don't record extended stats.
function test_extended_stats() {
  var h = Telemetry.getHistogramById("GRADIENT_DURATION");
  var s = h.snapshot();
  do_check_eq(s.sum, 0);
  do_check_eq(s.log_sum, 0);
  do_check_eq(s.log_sum_squares, 0);
  h.add(1);
  s = h.snapshot();
  do_check_eq(s.sum, 1);
  do_check_eq(s.log_sum, 0);
  do_check_eq(s.log_sum_squares, 0);
}

// Return an array of numbers from lower up to, excluding, upper
function numberRange(lower, upper)
{
  let a = [];
  for (let i=lower; i<upper; ++i) {
    a.push(i);
  }
  return a;
}

function test_keyed_boolean_histogram()
{
  const KEYED_ID = "test::keyed::boolean";
  const KEYS = ["key"+(i+1) for (i of numberRange(0, 3))];
  let histogramBase = {
    "min": 1,
    "max": 2,
    "histogram_type": 2,
    "sum": 1,
    "sum_squares_lo": 1,
    "sum_squares_hi": 0,
    "ranges": [0, 1, 2],
    "counts": [0, 1, 0]
  };
  let testHistograms = [JSON.parse(JSON.stringify(histogramBase)) for (i of numberRange(0, 3))];
  let testKeys = [];
  let testSnapShot = {};

  let h = Telemetry.newKeyedHistogram(KEYED_ID, "never", Telemetry.HISTOGRAM_BOOLEAN);
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
  testSnapShot[key].sum_squares_lo = 0;
  testSnapShot[key].counts = [1, 0, 0];
  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let allSnapshots = Telemetry.keyedHistogramSnapshots;
  Assert.deepEqual(allSnapshots[KEYED_ID], testSnapShot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
}

function test_keyed_count_histogram()
{
  const KEYED_ID = "test::keyed::count";
  const KEYS = ["key"+(i+1) for (i of numberRange(0, 5))];
  let histogramBase = {
    "min": 1,
    "max": 2,
    "histogram_type": 4,
    "sum": 0,
    "sum_squares_lo": 0,
    "sum_squares_hi": 0,
    "ranges": [0, 1, 2],
    "counts": [1, 0, 0]
  };
  let testHistograms = [JSON.parse(JSON.stringify(histogramBase)) for (i of numberRange(0, 5))];
  let testKeys = [];
  let testSnapShot = {};

  let h = Telemetry.newKeyedHistogram(KEYED_ID, "never", Telemetry.HISTOGRAM_COUNT);
  for (let i=0; i<4; ++i) {
    let key = KEYS[i];
    let value = i*2 + 1;

    for (let k=0; k<value; ++k) {
      h.add(key);
    }
    testHistograms[i].counts[0] = value;
    testHistograms[i].sum = value;
    testHistograms[i].sum_squares_lo = value;
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
  testHistograms[4].sum_squares_lo = 1;
  testSnapShot[key] = testHistograms[4];

  Assert.deepEqual(h.keys().sort(), testKeys);
  Assert.deepEqual(h.snapshot(), testSnapShot);

  let allSnapshots = Telemetry.keyedHistogramSnapshots;
  Assert.deepEqual(allSnapshots[KEYED_ID], testSnapShot);

  h.clear();
  Assert.deepEqual(h.keys(), []);
  Assert.deepEqual(h.snapshot(), {});
}

function test_keyed_flag_histogram()
{
  const KEYED_ID = "test::keyed::flag";
  let h = Telemetry.newKeyedHistogram(KEYED_ID, "never", Telemetry.HISTOGRAM_FLAG);

  const KEY = "default";
  h.add(KEY, true);

  let testSnapshot = {};
  testSnapshot[KEY] = {
    "min": 1,
    "max": 2,
    "histogram_type": 3,
    "sum": 1,
    "sum_squares_lo": 1,
    "sum_squares_hi": 0,
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
}

function test_keyed_histogram() {
  // Check that invalid names get rejected.

  let threw = false;
  try {
    Telemetry.newKeyedHistogram("test::invalid # histogram", "never", Telemetry.HISTOGRAM_BOOLEAN);
  } catch (e) {
    // This should throw as we reject names with the # separator
    threw = true;
  }
  Assert.ok(threw, "newKeyedHistogram should have thrown");

  threw = false;
  try {
    Telemetry.getKeyedHistogramById("test::unknown histogram", "never", Telemetry.HISTOGRAM_BOOLEAN);
  } catch (e) {
    // This should throw as it is an unknown ID
    threw = true;
  }
  Assert.ok(threw, "getKeyedHistogramById should have thrown");

  // Check specific keyed histogram types working properly.

  test_keyed_boolean_histogram();
  test_keyed_count_histogram();
  test_keyed_flag_histogram();
}

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

function run_test()
{
  let kinds = [Telemetry.HISTOGRAM_EXPONENTIAL, Telemetry.HISTOGRAM_LINEAR]
  for each (let histogram_type in kinds) {
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10]
    test_histogram(histogram_type, "test::"+histogram_type, min, max, bucket_count);

    const nh = Telemetry.newHistogram;
    expect_fail(function () nh("test::min", "never", histogram_type, 0, max, bucket_count));
    expect_fail(function () nh("test::bucket_count", "never", histogram_type, min, max, 1));
  }

  // Instantiate the storage for this histogram and make sure it doesn't
  // get reflected into JS, as it has no interesting data in it.
  let h = Telemetry.getHistogramById("NEWTAB_PAGE_PINNED_SITES_COUNT");
  do_check_false("NEWTAB_PAGE_PINNED_SITES_COUNT" in Telemetry.histogramSnapshots);

  test_boolean_histogram();
  test_flag_histogram();
  test_count_histogram();
  test_getHistogramById();
  test_histogramFrom();
  test_getSlowSQL();
  test_privateMode();
  test_addons();
  test_extended_stats();
  test_expired_histogram();
  test_keyed_histogram();
}
