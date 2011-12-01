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

var EXPORTED_SYMBOLS = ['TestSuite'];

const gIOS = Components.classes['@mozilla.org/network/io-service;1']
                               .getService(Components.interfaces.nsIIOService);
const scriptLoader = Components.classes['@mozilla.org/moz/jssubscript-loader;1']
                       .getService(Components.interfaces.mozIJSSubScriptLoader);

var api = {}; // api that gets injected into each test scope
var log = {};
var utils = {};
Components.utils.import('resource://pep/api.js', api);
Components.utils.import('resource://pep/logger.js', log);
Components.utils.import('resource://pep/utils.js', utils);


/**
 * Test Harness object for running tests and
 * logging results
 *
 * tests - a list of test objects to run
 */
function TestSuite(tests) {
  this.tests = tests;
}

TestSuite.prototype.run = function() {
  for (let i = 0; i < this.tests.length; ++i) {
    this.loadTest(this.tests[i]);
    // Sleep for a second because tests will interfere
    // with each other if loaded too quickly
    // TODO Figure out why they interfere with each other
    utils.sleep(1000);
  }
  log.info('Test Suite Finished');
};

/**
 * Takes a path to a js file and executes it in chrome scope,
 * loading in the pep API
 */
TestSuite.prototype.loadTest = function(test) {
  let file = Components.classes['@mozilla.org/file/local;1']
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(test.path);
  let uri = gIOS.newFileURI(file).spec;

  try {
    let testScope = {
      pep: new api.PepAPI(test)
    };

    // pre-test
    log.log('TEST-START', test.name);
    let startTime = Date.now();

    // run the test
    scriptLoader.loadSubScript(uri, testScope);

    // post-test
    let runTime = Date.now() - startTime;
    let fThreshold = test['failThreshold'] === undefined ?
                          '' : ' ' + test['failThreshold'];
    log.log('TEST-END', test.name + ' ' + runTime + fThreshold);
  } catch (e) {
    log.error(test.name + ' | ' + e);
    log.debug(test.name + ' | Traceback:');
    lines = e.stack.split('\n');
    for (let i = 0; i < lines.length - 1; ++i) {
      log.debug('\t' + lines[i]);
    }
  }
};
