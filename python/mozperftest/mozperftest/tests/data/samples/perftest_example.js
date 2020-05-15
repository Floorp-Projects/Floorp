// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */
"use strict";

var someVar;

async function setUp(context) {
  context.log.info("setUp example!");
}

async function test(context, commands) {
  context.log.info("Test with setUp/tearDown example!");
  await commands.measure.start("https://www.sitespeed.io/");
  await commands.measure.start("https://www.mozilla.org/en-US/");
}

async function tearDown(context) {
  context.log.info("tearDown example!");
}


module.exports = {
  setUp,
  tearDown,
  test,
  owner: "Performance Team",
  description: "Measures cold process applink time",
  long_description: `
  This test launches the appropriate android app, simulating an app link
  workflow. The application is launched with the intent action
  android.intent.action.VIEW loading a trivially simple website. The reported
  metric is the time from process start to navigationStart, reported as processLaunchToNavStart
  `,
  usage: `
  ./mach perftest testing/performance/perftest_applink.js \
    --android-install-apk ~/fenix.v2.fennec-nightly.2020.04.22-arm32.apk \
    --hooks testing/performance/hooks_applink.py \
    --android-app-name org.mozilla.fennec_aurora \
    --perfherder-metrics processLaunchToNavStart
  `,
  supported_browser: ["Fenix nightly", "Geckoview_example", "Fennec"],
  platform: ["Android"],
};
