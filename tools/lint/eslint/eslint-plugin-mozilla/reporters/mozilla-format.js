/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file outputs the format that treeherder requires. If we integrate
 * these tests with ./mach, then we may replace this with a json handler within
 * mach itself.
 */

"use strict";

var mocha = require("mocha");
var path = require("path");
module.exports = MozillaFormatter;

function MozillaFormatter(runner) {
  mocha.reporters.Base.call(this, runner);
  var passes = 0;
  var failures = [];

  runner.on("start", () => {
    console.log("SUITE-START | eslint-plugin-mozilla");
  });

  runner.on("pass", function (test) {
    passes++;
    let title = test.title.replace(/\n/g, "|");
    console.log(`TEST-PASS | ${path.basename(test.file)} | ${title}`);
  });

  runner.on("fail", function (test, err) {
    failures.push(test);
    // Replace any newlines in the title.
    let title = test.title.replace(/\n/g, "|");
    console.log(
      `TEST-UNEXPECTED-FAIL | ${path.basename(test.file)} | ${title} | ${
        err.message
      }`
    );
    mocha.reporters.Base.list([test]);
  });

  runner.on("end", function () {
    // Space the results out visually with an additional blank line.
    console.log("");
    console.log("INFO | Result summary:");
    console.log(`INFO | Passed: ${passes}`);
    console.log(`INFO | Failed: ${failures.length}`);
    console.log("SUITE-END");
    // Space the failures out visually with an additional blank line.
    console.log("");
    console.log("Failure summary:");
    mocha.reporters.Base.list(failures);
    process.exit(failures.length);
  });
}
