/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported PerTestCoverageUtils */

"use strict";

var EXPORTED_SYMBOLS = ["PerTestCoverageUtils"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
// This is the directory where gcov is emitting the gcda files.
const gcovPrefixPath = env.get("GCOV_PREFIX");
// This is the directory where codecoverage.py is expecting to see the gcda files.
const gcovResultsPath = env.get("GCOV_RESULTS_DIR");

const gcovPrefixDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
if (gcovPrefixPath) {
  gcovPrefixDir.initWithPath(gcovPrefixPath);
}

let gcovResultsDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
if (gcovResultsPath) {
  gcovResultsDir.initWithPath(gcovResultsPath);
}

function awaitPromise(promise) {
  let ret;
  let complete = false;
  let error = null;
  promise.catch(e => error = e).then(v => {
    ret = v;
    complete = true;
  });
  Services.tm.spinEventLoopUntil(() => complete);
  if (error) {
    throw new Error(error);
  }
  return ret;
}

function removeDirectoryContents(dir) {
  let entries = dir.directoryEntries;
  while (entries.hasMoreElements()) {
    entries.nextFile.remove(true);
  }
}

function moveDirectoryContents(src, dst) {
  let entries = src.directoryEntries;
  while (entries.hasMoreElements()) {
    entries.nextFile.moveTo(dst, null);
  }
}

var PerTestCoverageUtils = class PerTestCoverageUtilsClass {
  // Resets the counters to 0.
  static async beforeTest() {
    if (!PerTestCoverageUtils.enabled) {
      return;
    }

    // Flush the counters.
    let codeCoverageService = Cc["@mozilla.org/tools/code-coverage;1"].getService(Ci.nsICodeCoverage);
    await codeCoverageService.flushCounters();

    // Remove gcda files created by the flush, and those that might have been created between the end of a previous test and the beginning of the next one (e.g. some tests can create a new content process for every sub-test).
    removeDirectoryContents(gcovPrefixDir);

    // Move gcda files from the GCOV_RESULTS_DIR directory, so we can accumulate the counters.
    moveDirectoryContents(gcovResultsDir, gcovPrefixDir);
  }

  static beforeTestSync() {
    awaitPromise(this.beforeTest());
  }

  // Dumps counters and moves the gcda files in the directory expected by codecoverage.py.
  static async afterTest() {
    if (!PerTestCoverageUtils.enabled) {
      return;
    }

    // Flush the counters.
    let codeCoverageService = Cc["@mozilla.org/tools/code-coverage;1"].getService(Ci.nsICodeCoverage);
    await codeCoverageService.flushCounters();

    // Move the gcda files in the GCOV_RESULTS_DIR, so that the execution from now to shutdown (or next test) is not counted.
    moveDirectoryContents(gcovPrefixDir, gcovResultsDir);
  }

  static afterTestSync() {
    awaitPromise(this.afterTest());
  }
};

PerTestCoverageUtils.enabled = !!gcovResultsPath;
