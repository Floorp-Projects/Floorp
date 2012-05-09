/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['PepAPI'];
var results = {}; Components.utils.import('resource://pep/results.js', results);
var log = {};     Components.utils.import('resource://pep/logger.js', log);
var utils = {};   Components.utils.import('resource://pep/utils.js', utils);
var mozmill = {}; Components.utils.import('resource://mozmill/driver/mozmill.js', mozmill);
var securableModule = {};
Components.utils.import('resource://mozmill/stdlib/securable-module.js', securableModule);

const wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
const ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);

/**
 * This is the API exposed to tests
 * Any properties of this object will be injected into test scope
 * under the 'pep' namespace.
 */
function PepAPI(test, options) {
  this.test = test;
  this.options = options;
  this.log = new Log(this.test.name);
  this.resultHandler = new results.ResultHandler(this.test.name);

  this.file = Components.classes["@mozilla.org/file/local;1"]
                        .createInstance(Components.interfaces.nsILocalFile);
  this.file.initWithPath(this.test.path);
}
/**
 * Performs an action during which responsiveness is measured
 */
PepAPI.prototype.performAction = function(actionName, func) {
  this.resultHandler.startAction(actionName);
  try {
    func();
  } catch (e) {
    log.error(test.name + ' | ' + e);
    if (e['stack'] !== undefined) {
      log.debug(test.name + ' | Traceback:');
      let lines = e.stack.split('\n');
      for (let i = 0; i < lines.length - 1; ++i) {
        log.debug('\t' + lines[i]);
      }
    }
  }
  this.resultHandler.endAction();
};
/**
 * Returns the most recently used window of windowType
 */
PepAPI.prototype.getWindow = function(windowType) {
  if (windowType === undefined) {
    windowType = "navigator:browser";
  }
  return wm.getMostRecentWindow(windowType);
};
/**
 * Load a file on the local filesystem
 * module - path on the local file of the module to load (no extension)
 */
PepAPI.prototype.require = function(module) {
  let loader = new securableModule.Loader({
    rootPaths: [ios.newFileURI(this.file.parent).spec],
    defaultPrincipal: "system",
    globals: { Cc: Components.classes,
               Ci: Components.interfaces,
               Cr: Components.results,
               Cu: Components.utils,
               // mozmill scopes for compatibility with mozmill shared libraries
               // https://developer.mozilla.org/en/Mozmill_Tests/Shared_Modules
               mozmill: mozmill,
               // quick hack to keep backwards compatibility with mozmill 1.5.x
               elementslib: mozmill.findElement,
               findElement: mozmill.findElement,
               persisted: {},
             },
  });
  return loader.require(module);
};

/**
 * Sleep for a number of milliseconds
 */
PepAPI.prototype.sleep = function(milliseconds) {
  utils.sleep(milliseconds);
};

/**
 * Logging wrapper for tests
 */
function Log(testName) {
  this.testName = testName;
}
Log.prototype.debug = function(msg) {
  log.debug(this.testName + ' | ' + msg);
};
Log.prototype.info = function(msg) {
  log.info(this.testName + ' | ' + msg);
};
Log.prototype.warning = function(msg) {
  log.warning(this.testName + ' | ' + msg);
};
Log.prototype.error = function(msg) {
  log.error(this.testName + ' | ' + msg);
};
