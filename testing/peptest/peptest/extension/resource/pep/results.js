/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is peptest.
*
* The Initial Developer of the Original Code is
*   The Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2011.
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*   Andrew Halberstadt <halbersa@gmail.com>
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
***** END LICENSE BLOCK ***** */

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
  this.currentAction['test_name'] = this.testName
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
  return this.results
}
