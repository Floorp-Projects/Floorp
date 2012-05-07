/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
function TestSuite(tests, options) {
  this.tests = tests;
  this.options = options;
  if (!this.options.numIterations) {
    this.options.numIterations = 1;
  }
}

TestSuite.prototype.run = function() {
  for (let i = 0; i < this.options.numIterations; ++i) {
    for (let j = 0; j < this.tests.length; ++j) {
      this.loadTest(this.tests[j]);
      // Sleep for a second because tests will interfere
      // with each other if loaded too quickly
      // TODO Figure out why they interfere with each other
      utils.sleep(1000);
    }
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
      pep: new api.PepAPI(test, this.options)
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
    if (e['stack'] !== undefined) {
      log.debug(test.name + ' | Traceback:');
      lines = e.stack.split('\n');
      for (let i = 0; i < lines.length - 1; ++i) {
        log.debug('\t' + lines[i]);
      }
    }
  }
};
