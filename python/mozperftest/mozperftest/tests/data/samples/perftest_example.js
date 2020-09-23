// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */
"use strict";

var someVar;

someVar = 2;

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

module.noexport = {};

module.exports = {
  setUp,
  tearDown,
  test,
  owner: "Performance Testing Team",
  name: "Example",
  description: "The description of the example test.",
  longDescription: `
  This is a longer description of the test perhaps including information
  about how it should be run locally or links to relevant information.
  `,
  supportedBrowsers: ["Fenix nightly", "Geckoview_example", "Fennec", "Firefox"],
  supportedPlatforms: ["Android", "Desktop"],
  options: {
      default: {perfherder: true, verbose: false},
      linux: {perfherder_metrics: [{name:"speed",unit:"bps_lin"}], verbose: true},
      mac: {perfherder_metrics: [{name:"speed",unit:"bps_mac"}], verbose: true},
      win: {perfherder_metrics: [{name:"speed",unit:"bps_win"}], verbose: true}
  }
};
