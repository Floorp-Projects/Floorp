/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['ResultHandler'];

var log = {}; Components.utils.import('resource://pep/logger.js', log);
var utils = {}; Components.utils.import('resource://pep/utils.js', utils);

function ResultHandler(testName) {
  this.results = [];
  this.currentAction = {};
  this.testName = testName;
}

ResultHandler.prototype.startAction = function(actionName) {
  this.currentAction = {};
  this.currentAction['test_name'] = this.testName;
  this.currentAction['action_name'] = actionName;
  this.currentAction['start_time'] = Date.now();
  log.log('ACTION-START', this.testName + ' ' + this.currentAction['action_name']);
}

ResultHandler.prototype.endAction = function() {
  if (this.currentAction['start_time']) {
    // Sleep for 200 milliseconds
    // This is here because there may still be event tracer events
    // propagating through the event loop. We don't want to miss
    // unresponsiveness caused by operations executed towards the
    // end of a performAction call.
    utils.sleep(200);
    this.currentAction['end_time'] = Date.now();
    this.results.push(this.currentAction);
    log.log('ACTION-END', this.testName + ' ' + this.currentAction['action_name']);
  }
}

ResultHandler.prototype.getResults = function() {
  return this.results;
}
