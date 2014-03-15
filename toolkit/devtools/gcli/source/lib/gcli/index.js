/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var Cc = require('chrome').Cc;
var Ci = require('chrome').Ci;
var Cu = require('chrome').Cu;

/*
 * GCLI is built from a number of components (called items) composed as
 * required for each environment.
 * When adding to or removing from this list, we should keep the basics in sync
 * with the other environments.
 * See:
 * - lib/gcli/index.js: Generic basic set (without commands)
 * - lib/gcli/demo.js: Adds demo commands to basic set for use in web demo
 * - gcli.js: Add commands to basic set for use in Node command line
 * - lib/gcli/index.js: (mozmaster branch) From scratch listing for Firefox
 * - lib/gcli/connectors/index.js: Client only items when executing remotely
 * - lib/gcli/connectors/direct.js: Test items for connecting to in-process GCLI
 */
var items = [
  require('./types/delegate').items,
  require('./types/selection').items,
  require('./types/array').items,

  require('./types/boolean').items,
  require('./types/command').items,
  require('./types/date').items,
  require('./types/file').items,
  require('./types/javascript').items,
  require('./types/node').items,
  require('./types/number').items,
  require('./types/resource').items,
  require('./types/setting').items,
  require('./types/string').items,

  require('./fields/delegate').items,
  require('./fields/selection').items,

  require('./ui/focus').items,
  require('./ui/intro').items,

  require('./converters/converters').items,
  require('./converters/basic').items,
  // require('./converters/html').items, // Prevent use of innerHTML
  require('./converters/terminal').items,

  require('./languages/command').items,
  require('./languages/javascript').items,

  // require('./connectors/direct').items, // No need for loopback testing
  // require('./connectors/rdp').items, // Needs fixing
  // require('./connectors/websocket').items, // Not from chrome
  // require('./connectors/xhr').items, // Not from chrome

  // require('./cli').items, // No need for '{' with web console
  require('./commands/clear').items,
  // require('./commands/connect').items, // We need to fix our RDP connector
  require('./commands/context').items,
  // require('./commands/exec').items, // No exec in Firefox yet
  require('./commands/global').items,
  require('./commands/help').items,
  // require('./commands/intro').items, // No need for intro command
  require('./commands/lang').items,
  // require('./commands/mocks').items, // Only for testing
  require('./commands/pref').items,
  // require('./commands/preflist').items, // Too slow in Firefox
  // require('./commands/test').items, // Only for testing

  // No demo or node commands

].reduce(function(prev, curr) { return prev.concat(curr); }, []);

var api = require('./api');
api.populateApi(exports);
exports.addItems(items);

var host = require('./util/host');

exports.useTarget = host.script.useTarget;

/**
 * This code is internal and subject to change without notice.
 * createDisplay() for Firefox requires an options object with the following
 * members:
 * - contentDocument: From the window of the attached tab
 * - chromeDocument: GCLITerm.document
 * - environment.hudId: GCLITerm.hudId
 * - jsEnvironment.globalObject: 'window'
 * - jsEnvironment.evalFunction: 'eval' in a sandbox
 * - inputElement: GCLITerm.inputNode
 * - completeElement: GCLITerm.completeNode
 * - hintElement: GCLITerm.hintNode
 * - inputBackgroundElement: GCLITerm.inputStack
 */
exports.createDisplay = function(opts) {
  var FFDisplay = require('./mozui/ffdisplay').FFDisplay;
  return new FFDisplay(opts);
};

var prefSvc = Cc['@mozilla.org/preferences-service;1']
                        .getService(Ci.nsIPrefService);
var prefBranch = prefSvc.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);

exports.hiddenByChromePref = function() {
  return !prefBranch.prefHasUserValue('devtools.chrome.enabled');
};


try {
  var Services = Cu.import('resource://gre/modules/Services.jsm', {}).Services;
  var stringBundle = Services.strings.createBundle(
          'chrome://browser/locale/devtools/gclicommands.properties');

  /**
   * Lookup a string in the GCLI string bundle
   */
  exports.lookup = function(name) {
    try {
      return stringBundle.GetStringFromName(name);
    }
    catch (ex) {
      throw new Error('Failure in lookup(\'' + name + '\')');
    }
  };

  /**
   * Lookup a string in the GCLI string bundle
   */
  exports.lookupFormat = function(name, swaps) {
    try {
      return stringBundle.formatStringFromName(name, swaps, swaps.length);
    }
    catch (ex) {
      throw new Error('Failure in lookupFormat(\'' + name + '\')');
    }
  };
}
catch (ex) {
  console.error('Using string fallbacks', ex);

  exports.lookup = function(name) {
    return name;
  };
  exports.lookupFormat = function(name, swaps) {
    return name;
  };
}
