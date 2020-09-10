/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var perfMetadata = {
  owner: "Performance Testing Team",
  name: "Example",
  description: "The description of the example test.",
  longDescription: `
  This is a longer description of the test perhaps including information
  about how it should be run locally or links to relevant information.
  `,
  usage: `
  ./mach perftest
    python/mozperftest/mozperftest/tests/data/samples/test_xpcshell_flavor2.js
  `,
  supportedBrowsers: ["Firefox"],
  supportedPlatforms: ["Desktop"],
};

function run_next_test() {
  // do something
}

function run_test() {
  // do something
}
