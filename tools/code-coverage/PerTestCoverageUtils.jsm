/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported PerTestCoverageUtils */

"use strict";

var EXPORTED_SYMBOLS = ["PerTestCoverageUtils"];

ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
// This is the directory where gcov is emitting the gcda files.
const tmp_gcov_dir = env.get("GCOV_PREFIX");
// This is the directory where codecoverage.py is expecting to see the gcda files.
const gcov_dir = env.get("GCOV_RESULTS_DIR");

const enabled = !!gcov_dir;

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

var PerTestCoverageUtils = class PerTestCoverageUtilsClass {
  // Resets the counters to 0.
  static async beforeTest() {
    if (!enabled) {
      return;
    }

    let codeCoverageService = Cc["@mozilla.org/tools/code-coverage;1"].getService(Ci.nsICodeCoverage);
    await codeCoverageService.resetCounters();
  }

  static beforeTestSync() {
    awaitPromise(this.beforeTest());
  }

  // Dumps counters and moves the gcda files in the directory expected by codecoverage.py.
  static async afterTest() {
    if (!enabled) {
      return;
    }

    let codeCoverageService = Cc["@mozilla.org/tools/code-coverage;1"].getService(Ci.nsICodeCoverage);
    await codeCoverageService.dumpCounters();

    let srcDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    srcDir.initWithPath(tmp_gcov_dir);

    let destDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    destDir.initWithPath(gcov_dir);

    let srcDirEntries = srcDir.directoryEntries;
    while (srcDirEntries.hasMoreElements()) {
      srcDirEntries.nextFile.moveTo(destDir, null);
    }
  }

  static afterTestSync() {
    awaitPromise(this.afterTest());
  }
};
