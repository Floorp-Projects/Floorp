/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "CoverageCollector",
]

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const {TextEncoder, OS} = Cu.import("resource://gre/modules/osfile.jsm", {});
const {addDebuggerToGlobal} = Cu.import("resource://gre/modules/jsdebugger.jsm",
                                        {});
addDebuggerToGlobal(this);

/**
 * Records coverage for each test by way of the js debugger.
 */
this.CoverageCollector = function (prefix) {
  this._prefix = prefix;
  this._dbg = new Debugger();
  this._dbg.collectCoverageInfo = true;
  this._dbg.addAllGlobalsAsDebuggees();
  this._scripts = this._dbg.findScripts();

  this._dbg.onNewScript = (script) => {
    this._scripts.push(script);
  };

  // Source -> coverage data;
  this._allCoverage = {};
  this._encoder = new TextEncoder();
  this._testIndex = 0;
}

CoverageCollector.prototype._getLinesCovered = function () {
  let coveredLines = {};
  let currentCoverage = {};
  this._scripts.forEach(s => {
    let scriptName = s.url;
    let cov = s.getOffsetsCoverage();
    if (!cov) {
      return;
    }

    cov.forEach(covered => {
      let {lineNumber, columnNumber, offset, count} = covered;
      if (!count) {
        return;
      }

      if (!currentCoverage[scriptName]) {
        currentCoverage[scriptName] = {};
      }
      if (!this._allCoverage[scriptName]) {
        this._allCoverage[scriptName] = {};
      }

      let key = [lineNumber, columnNumber, offset].join('#');
      if (!currentCoverage[scriptName][key]) {
        currentCoverage[scriptName][key] = count;
      } else {
        currentCoverage[scriptName][key] += count;
      }
    });

  });

  // Covered lines are determined by comparing every offset mentioned as of the
  // the completion of a test to the last time we measured coverage. If an
  // offset in a line is novel as of this test, or a count has increased for
  // any offset on a particular line, that line must have been covered.
  for (let scriptName in currentCoverage) {
    for (let key in currentCoverage[scriptName]) {
      if (!this._allCoverage[scriptName] ||
          !this._allCoverage[scriptName][key] ||
          (this._allCoverage[scriptName][key] <
           currentCoverage[scriptName][key])) {
        let [lineNumber, colNumber, offset] = key.split('#');
        if (!coveredLines[scriptName]) {
          coveredLines[scriptName] = new Set();
        }
        coveredLines[scriptName].add(parseInt(lineNumber, 10));
        this._allCoverage[scriptName][key] = currentCoverage[scriptName][key];
      }
    }
  }

  return coveredLines;
}

CoverageCollector.prototype._getUncoveredLines = function() {
  let uncoveredLines = {};
  this._scripts.forEach(s => {
    let scriptName = s.url;
    let scriptOffsets = s.getAllOffsets();

    if (!uncoveredLines[scriptName]){
      uncoveredLines[scriptName] = new Set();
    }

    // Get all lines in the script
    scriptOffsets.forEach( function(element, index) {
      if (!element){
        return;
      }
      uncoveredLines[scriptName].add(index);
    });
  });

  // For all covered lines, delete their entry
  for (let scriptName in this._allCoverage){
    for (let key in this._allCoverage[scriptName]){
      let [lineNumber, columnNumber, offset] = key.split('#');
      uncoveredLines[scriptName].delete(parseInt(lineNumber, 10));
    }
  }

  return uncoveredLines;
}

CoverageCollector.prototype._getMethodNames = function() {
  let methodNames = {};
  this._scripts.forEach(s => {
    let method = s.displayName;
    // If the method name is undefined, we return early
    if (!method){
      return;
    }

    let scriptName = s.url;
    let tempMethodCov = [];
    let scriptOffsets = s.getAllOffsets();

    if (!methodNames[scriptName]){
      methodNames[scriptName] = {};
    }

    /**
    * Get all lines contained within the method and
    * push a record of the form:
    * <method name> : <lines covered>
    */
    scriptOffsets.forEach(function (element, index){
      if (!element){
        return;
      }
      tempMethodCov.push(index);
    });
    methodNames[scriptName][method] = tempMethodCov;
  });

  return methodNames;
}

/**
 * Records lines covered since the last time coverage was recorded,
 * associating them with the given test name. The result is written
 * to a json file in a specified directory.
 */
CoverageCollector.prototype.recordTestCoverage = function (testName) {
  dump("Collecting coverage for: " + testName + "\n");
  let rawLines = this._getLinesCovered(testName);
  let methods = this._getMethodNames();
  let uncoveredLines = this._getUncoveredLines();
  let result = [];
  let versionControlBlock = {version: 1.0};
  result.push(versionControlBlock);

  for (let scriptName in rawLines) {
    let rec = {
      testUrl: testName,
      sourceFile: scriptName,
      methods: {},
      covered: [],
      uncovered: []
    };

    for (let methodName in methods[scriptName]){
      rec.methods[methodName] = methods[scriptName][methodName];
    }

    for (let line of rawLines[scriptName]) {
      rec.covered.push(line);
    }

    for (let line of uncoveredLines[scriptName]){
      rec.uncovered.push(line);
    }

    result.push(rec);
  }
  let arr = this._encoder.encode(JSON.stringify(result, null, 2));
  let path = this._prefix + '/' + 'jscov_' + Date.now() + '.json';
  dump("Writing coverage to: " + path + "\n");
  return OS.File.writeAtomic(path, arr, {tmpPath: path + '.tmp'});
}

/**
 * Tear down the debugger after all tests are complete.
 */
CoverageCollector.prototype.finalize = function () {
  this._dbg.removeAllDebuggees();
  this._dbg.enabled = false;
}
