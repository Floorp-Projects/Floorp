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
  var failures = 0;

  runner.on("start", () => {
    console.log("SUITE-START | eslint-plugin-mozilla");
  });

  runner.on("pass", function(test) {
    passes++;
    let title = test.title.replace(/\n/g, "|");
    console.log(`TEST-PASS | ${path.basename(test.file)} | ${title}`);
  });

  runner.on("fail", function(test, err) {
    failures++;
    // Replace any newlines in the title.
    let title = test.title.replace(/\n/g, "|");
    console.log(`TEST-UNEXPECTED-FAIL | ${path.basename(test.file)} | ${title} | ${err.message}`);
  });

  runner.on("end", function() {
    console.log("INFO | Result summary:");
    console.log(`INFO | Passed: ${passes}`);
    console.log(`INFO | Failed: ${failures}`);
    console.log("SUITE-END");
    process.exit(failures);
  });
}
