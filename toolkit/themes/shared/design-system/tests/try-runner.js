/* eslint-disable no-console */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

/*
 * A small test runner/reporter for node-based tests,
 * which are run via taskcluster node(debugger).
 *
 * Adapted from:
 * https://searchfox.org/mozilla-central/rev/9cd4ea81e27db6b767f1d9bbbcf47da238dd64fa/browser/components/newtab/bin/try-runner.js
 */

const { readFileSync, rmSync } = require("fs");
const chalk = require("chalk");
const path = require("path");
const StyleDictionary = require("style-dictionary");
const config = require("../tokens-config.js");

const TEST_BUILD_PATH = "tests/build/css/";

function buildFilesWithTestConfig() {
  // Use our real config, just modify some values for the test. This prevents us
  // from re-building the CSS files that get checked in when we run the tests.
  let testConfig = Object.assign({}, config);
  testConfig.source = [path.join(__dirname, "../design-tokens.json")];
  testConfig.platforms.css.buildPath = TEST_BUILD_PATH;

  // This is effectively the same as running `npm run build` and allows us to
  // use the modified config.
  StyleDictionary.extend(testConfig).buildAllPlatforms();
}

function logErrors(tool, errors) {
  for (const error of errors) {
    console.log(`TEST-UNEXPECTED-FAIL | ${tool} | ${error}`);
  }
  return errors;
}

function logStart(name) {
  console.log(`TEST-START | ${name}`);
}

const FILE_PATHS = {
  "tokens-brand.css": {
    path: path.join("tokens-brand.css"),
    testPath: path.join(TEST_BUILD_PATH, "tokens-brand.css"),
  },
  "tokens-platform.css": {
    path: path.join("tokens-platform.css"),
    testPath: path.join(TEST_BUILD_PATH, "tokens-platform.css"),
  },
  "tokens-shared.css": {
    path: path.join("tokens-shared.css"),
    testPath: path.join(TEST_BUILD_PATH, "tokens-shared.css"),
  },
};

const tests = {
  // Verify the CSS files build successfully and are up to date.
  buildCSS() {
    logStart("build CSS");

    let errors = [];
    let currentCSS = {};

    // Read the contents of our built CSS files.
    for (let [fileName, { path }] of Object.entries(FILE_PATHS)) {
      currentCSS[fileName] = readFileSync(path, "utf8");
    }

    try {
      buildFilesWithTestConfig();
    } catch {
      errors.push("CSS build did not run successfully");
    }

    // Build CSS files to the test directory and compare them to the current CSS
    // files that get checked in. If the contents don't match we either forgot
    // to build the files after making a change, or edited the CSS files directly.
    for (let [fileName, { testPath }] of Object.entries(FILE_PATHS)) {
      let builtCSS = readFileSync(testPath, "utf8");

      if (builtCSS !== currentCSS[fileName]) {
        errors.push(`${fileName} is out of date`);
      }

      if (builtCSS.includes("/** Unspecified **/")) {
        errors.push(
          "Tokens present in the 'Unspecified' section. Please update TOKEN_SECTIONS in tokens-config.js"
        );
      }
    }

    logErrors("build CSS", errors);
    rmSync("tests/build", { recursive: true, force: true });
    return errors.length === 0;
  },
};

(function runTests() {
  let results = [];

  for (let testName of Object.keys(tests)) {
    results.push([testName, tests[testName]()]);
  }

  for (const [name, result] of results) {
    // Colorize output based on result.
    console.log(result ? chalk.green(`✓ ${name}`) : chalk.red(`✗ ${name}`));
  }

  const success = results.every(([, result]) => result);
  process.exitCode = success ? 0 : 1;
  console.log("CODE", process.exitCode);
})();
