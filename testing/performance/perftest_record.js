/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */
"use strict";

async function test(context, commands) {
  let rootUrl = context.options.browsertime.url;
  let testName = context.options.browsertime.testName;

  await commands.measure.start(rootUrl);
  commands.screenshot.take(testName);

  // Wait for browser to settle
  await commands.wait.byTime(1000);
  return true;
}

module.exports = {
  test,
  owner: "Bebe fstrugariu@mozilla.com",
  name: "Mozproxy recording generator",
  component: "raptor",
  description: ` This test generates fresh MozProxy recordings. It iterates through a list of 
      websites provided in *_sites.json and for each one opens a browser and 
      records all the associated HTTP traffic`,
  usage:
    "mach perftest --proxy --hooks testing/raptor/recorder/hooks.py testing/raptor/recorder/perftest_record.js",
};
