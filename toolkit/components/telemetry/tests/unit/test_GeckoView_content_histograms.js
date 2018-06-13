/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);

// Note: this test file is only supposed to be run by
// test_GeckoView.js. It assumes to be in the content
// process.
function run_test() {
  // Get the histograms and set some values in the content process.
  Services.telemetry.getHistogramById("TELEMETRY_TEST_MULTIPRODUCT")
                    .add(73);
  Services.telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT")
                    .add("content-test-key", 37);
}
