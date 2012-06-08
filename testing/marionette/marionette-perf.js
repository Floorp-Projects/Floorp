/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function MarionettePerfData() {
  this.perfData = {};
}
MarionettePerfData.prototype = {
  /**
   * Add performance data. 
   *
   * Datapoints within a testSuite get rolled up into
   * one value in Datazilla. You can then drill down to
   * individual (testName,data) pairs
   * 
   * If the testSuite and testName exist, the data will
   * be added to this dataset.
   *
   * @param testSuite String
   *        name of the test suite
   * @param testName String
   *        name of the test
   * @param object data
   *        data value to store
   */
  addPerfData: function Marionette__addPerfData(testSuite, testName, data) {
    if (this.perfData[testSuite]) {
      if (this.perfData[testSuite][testName]) {
        this.perfData[testSuite][testName].push(data);
      }
      else {
        this.perfData[testSuite][testName.toString()] = [data];
      }
    }
    else {
      this.perfData[testSuite] = {}
      this.perfData[testSuite][testName.toString()] = [data];
    }
  },

  /**
   * Join another set of performance data this this set.
   * Used by the actor to join data gathered from the listener
   * @param object data
   *        The performance data to join
   */
  appendPerfData: function Marionette__appendPerfData(data) {
    for (var suite in data) {
      if (data.hasOwnProperty(suite)) {
        if (this.perfData[suite]) {
          for (var test in data[suite]) {
            if (this.perfData[suite][test]) {
              this.perfData[suite][test] = this.perfData[suite][test].concat(data[suite][test]);
            }
            else {
              this.perfData[suite][test] = data[suite][test];
            }
          }
        }
        else {
          this.perfData[suite] = data[suite];
        }
      }
    }
  },

  /**
   * Retrieve the performance data
   *        
   * @return object
   *      Returns a list of test names to metric value
   */
  getPerfData: function Marionette__getPerfData() {
    return this.perfData;
  },

  /**
   * Clears the current performance data
   */
  clearPerfData: function Marionette_clearPerfData() {
    this.perfData = {};
  },
}
