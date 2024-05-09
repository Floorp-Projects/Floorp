// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */
"use strict";

async function test() {}

module.exports = {
  test,
  owner: "Performance Team",
  name: "android-startup",
  description: "Measures android startup times",
  longDescription: `
  This test consists of 2 main tests, cold main first frame(cmff) and cold view nav start(cvns).
  cold main first frame is the measurement from when you click the app icon & get duration to first frame from 'am start -W'.
  cold view nav start is the measurement from when you send a VIEW intent & get duration from logcat: START proc to PageStart.
  `,
  usage: `
  ./mach perftest --flavor mobile-browser --AndroidStartUp testing/performance/perftest_android_startup.js
    --browsertime-cycles=0 --AndroidStartUp-test-name=cold_view_nav_start --perfherder --hooks
    testing/performance/hooks_android_startup.py --AndroidStartUp-product=fenix
    --AndroidStartUp-release-channel=nightly
  `,
  options: {
    test_parameters: {
      single_date: null, // Dates in YYYY.MM.DD format
      date_range: [], // 2 Dates in YYYY.MM.DD format the first and last date(inclusive)
      startup_cache: true,
      test_cycles: 50,
      release_channel: "nightly", // either release, nightly, beta, or debug
      architecture: "arm64-v8a",
    },
  },
};
