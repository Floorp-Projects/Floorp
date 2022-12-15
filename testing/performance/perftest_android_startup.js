// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */
"use strict";

async function test(context, commands) {}

module.exports = {
  test,
  owner: "Performance Team",
  name: "android-startup",
  description: "Measures cold process view time",
  longDescription: `
  This test launches the appropriate android app, simulating a opening a link through VIEW intent
  workflow. The application is launched with the intent action
  android.intent.action.VIEW loading a trivially simple website. The reported
  metric is the time from process start to navigationStart, reported as processLaunchToNavStart
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
    },
  },
};
