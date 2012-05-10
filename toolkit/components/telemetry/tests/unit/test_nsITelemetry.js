/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const INT_MAX = 0x7FFFFFFF;

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
Cu.import("resource://gre/modules/Services.jsm");

function test_histogram(histogram_type, name, min, max, bucket_count) {
  var h = Telemetry.newHistogram(name, min, max, bucket_count, histogram_type);
  
  var r = h.snapshot().ranges;
  var sum = 0;
  for(var i=0;i<r.length;i++) {
    var v = r[i];
    sum += v;
    h.add(v);
  }
  var s = h.snapshot();
  // verify sum
  do_check_eq(sum, h.snapshot().sum);;
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
  var h = Telemetry.newHistogram("test::boolean histogram", 99,1,4, Telemetry.HISTOGRAM_BOOLEAN);
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
  var h = Telemetry.newHistogram("test::flag histogram", 130, 4, 5, Telemetry.HISTOGRAM_FLAG);
  var r = h.snapshot().ranges;
  // Flag histograms ignore numeric parameters.
  do_check_eq(uneval(r), uneval([0, 1, 2]))
  // Should already have a 0 counted.
  var c = h.snapshot().counts;
  var s = h.snapshot().sum;
  do_check_eq(uneval(c), uneval([1, 0, 0]));
  do_check_eq(s, 1);
  // Should switch counts.
  h.add(2);
  var c2 = h.snapshot().counts;
  var s2 = h.snapshot().sum;
  do_check_eq(uneval(c2), uneval([0, 1, 0]));
  do_check_eq(s, 1);
  // Should only switch counts once.
  h.add(3);
  var c3 = h.snapshot().counts;
  var s3 = h.snapshot().sum;
  do_check_eq(uneval(c3), uneval([0, 1, 0]));
  do_check_eq(s3, 1);
  do_check_eq(h.snapshot().histogram_type, Telemetry.FLAG_HISTOGRAM);
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

  do_check_eq(s1.counts.length, s2.counts.length);
  for (let i = 0; i < s1.counts.length; i++)
    do_check_eq(s1.counts[i], s2.counts[i]);

  do_check_eq(s1.ranges.length, s2.ranges.length);
  for (let i = 0; i < s1.ranges.length; i++)
    do_check_eq(s1.ranges[i], s2.ranges[i]);
}

function test_histogramFrom() {
  // Test one histogram of each type.
  let names = ["CYCLE_COLLECTOR", "GC_REASON", "GC_RESET", "TELEMETRY_TEST_FLAG"];

  for each (let name in names) {
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10]
    let original = Telemetry.getHistogramById(name);
    let clone = Telemetry.histogramFrom("clone" + name, name);
    compareHistograms(original, clone);
  }

  // Additionally, set the flag on TELEMETRY_TEST_FLAG, and check it gets set on the clone.
  let testFlag = Telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  testFlag.add(1);
  let clone = Telemetry.histogramFrom("FlagClone", "TELEMETRY_TEST_FLAG");
  compareHistograms(testFlag, clone);
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
  var h = Telemetry.newHistogram("test::private_mode_boolean", 1,2,3, Telemetry.HISTOGRAM_BOOLEAN);
  var orig = h.snapshot();
  Telemetry.canRecord = false;
  h.add(1);
  do_check_eq(uneval(orig), uneval(h.snapshot()));
  Telemetry.canRecord = true;
  h.add(1);
  do_check_neq(uneval(orig), uneval(h.snapshot()));
}

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

// Check that we do sane things when saving to disk.
function test_loadSave()
{
  let dirService = Cc["@mozilla.org/file/directory_service;1"]
                    .getService(Ci.nsIProperties);
  let tmpDir = dirService.get("TmpD", Ci.nsILocalFile);
  let tmpFile = tmpDir.clone();
  tmpFile.append("saved-histograms.dat");
  if (tmpFile.exists()) {
    tmpFile.remove(true);
  }

  let saveFinished = false;
  let loadFinished = false;
  let uuid = generateUUID();
  let loadCallback = function(data) {
    do_check_true(data != null);
    do_check_eq(uuid, data.uuid);
    loadFinished = true;
    do_test_finished();
  };
  let saveCallback = function(success) {
    do_check_true(success);
    Telemetry.loadHistograms(tmpFile, loadCallback, false);
    saveFinished = true;
  };
  do_test_pending();
  Telemetry.saveHistograms(tmpFile, uuid, saveCallback, false);
  do_register_cleanup(function () do_check_true(saveFinished));
  do_register_cleanup(function () do_check_true(loadFinished));
  do_register_cleanup(function () tmpFile.remove(true));
}

function run_test()
{
  let kinds = [Telemetry.HISTOGRAM_EXPONENTIAL, Telemetry.HISTOGRAM_LINEAR]
  for each (let histogram_type in kinds) {
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10]
    test_histogram(histogram_type, "test::"+histogram_type, min, max, bucket_count);
    
    const nh = Telemetry.newHistogram;
    expect_fail(function () nh("test::min", 0, max, bucket_count, histogram_type));
    expect_fail(function () nh("test::bucket_count", min, max, 1, histogram_type));
  }

  // Instantiate the storage for this histogram and make sure it doesn't
  // get reflected into JS, as it has no interesting data in it.
  let h = Telemetry.getHistogramById("NEWTAB_PAGE_PINNED_SITES_COUNT");
  do_check_false("NEWTAB_PAGE_PINNED_SITES_COUNT" in Telemetry.histogramSnapshots);

  test_boolean_histogram();
  test_getHistogramById();
  test_histogramFrom();
  test_getSlowSQL();
  test_privateMode();
  test_addons();
  test_loadSave();
}
