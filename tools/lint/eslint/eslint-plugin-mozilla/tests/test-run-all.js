/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RuleTester = require("eslint/lib/testers/rule-tester");
const fs = require('fs');

var ruleTester = new RuleTester();

fs.readdir(__dirname, (err, files) => {
  files.forEach(file => {
    if (file != "test-run-all.js" && !file.endsWith("~")) {
      console.log(`Running ${file}`);
      require("./" + file).runTest(ruleTester);
    }
  });
});
