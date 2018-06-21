/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported PerTestCoverageUtils */

"use strict";

var EXPORTED_SYMBOLS = ["PerTestCoverageUtils"];

ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

class PerTestCoverageUtilsClass {
  constructor(tmp_gcov_dir, gcov_dir) {
    this.tmp_gcov_dir = tmp_gcov_dir;
    this.gcov_dir = gcov_dir;

    this.codeCoverageService = Cc["@mozilla.org/tools/code-coverage;1"].getService(Ci.nsICodeCoverage);
  }

  _awaitPromise(promise) {
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

  // Resets the counters to 0.
  beforeTest() {
    this._awaitPromise(this.codeCoverageService.resetCounters());
  }

  // Dumps counters and moves the gcda files in the directory expected by codecoverage.py.
  afterTest() {
    this._awaitPromise(this.codeCoverageService.dumpCounters());

    let srcDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    srcDir.initWithPath(this.tmp_gcov_dir);

    let destDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    destDir.initWithPath(this.gcov_dir);

    let srcDirEntries = srcDir.directoryEntries;
    while (srcDirEntries.hasMoreElements()) {
      srcDirEntries.nextFile.moveTo(destDir, null);
    }
  }
}

const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
// This is the directory where gcov is emitting the gcda files.
const tmp_gcov_dir = env.get("GCOV_PREFIX");
// This is the directory where codecoverage.py is expecting to see the gcda files.
const gcov_dir = env.get("GCOV_RESULTS_DIR");

const PerTestCoverageUtils = gcov_dir ? new PerTestCoverageUtilsClass(tmp_gcov_dir, gcov_dir) : null;
