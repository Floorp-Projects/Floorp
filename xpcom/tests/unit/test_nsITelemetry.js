/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);

function test_histogram(histogram_constructor, name, min, max, bucket_count) {
  var h = histogram_constructor(name, min, max, bucket_count);
  
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
  do_check_eq(gh.histogram_type, 
              histogram_constructor == Telemetry.newExponentialHistogram ? 0 : 1);
  do_check_eq(gh.min, min)
  do_check_eq(gh.max, max)
}

function run_test()
{
  test_histogram(Telemetry.newExponentialHistogram, "test::Exponential", 1, 10000, 10);
  test_histogram(Telemetry.newLinearHistogram, "test::Linear", 1, 10000, 10);
}
