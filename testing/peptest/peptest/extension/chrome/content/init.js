/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const cmdLineHandler =
        Cc["@mozilla.org/commandlinehandler/general-startup;1?type=pep"]
        .getService(Ci.nsICommandLineHandler);


var log = {};       // basic logger
var utils = {};     // utility object
var broker = {};    // mozmill message broker for listening to mozmill events
// test suite object that will run the tests
Components.utils.import('resource://pep/testsuite.js');
Components.utils.import('resource://pep/logger.js', log);
Components.utils.import('resource://pep/utils.js', utils);
Components.utils.import('resource://mozmill/driver/msgbroker.js', broker);

var APPCONTENT;

/**
 * This is the entry point for peptest.
 * Gets called when the browser is first loaded.
 */
function initialize() {
  window.removeEventListener("load", initialize, false);
  let cmd = cmdLineHandler.wrappedJSObject;
  // cmd.firstRun is used so the tests don't
  // get run again if a second window is opened
  if (cmd.firstRun) {
    cmd.firstRun = false;
    try {
      // get json manifest object
      let manifest = cmd.manifest;
      let data = utils.readFile(manifest);
      let obj = JSON.parse(data.join(' '));

      // register mozmill listener
      broker.addObject(new MozmillMsgListener());

      // set a load listener on the content and run the tests when loaded
      APPCONTENT = document.getElementById('appcontent');
      function runTests() {
        APPCONTENT.removeEventListener('pageshow', runTests);
        suite = new TestSuite(obj.tests, obj.options);
        suite.run();
        goQuitApplication();
      };
      APPCONTENT.addEventListener('pageshow', runTests);
    } catch(e) {
      log.error(e.toString());
      log.debug('Traceback:');
      lines = e.stack.split('\n');
      for (let i = 0; i < lines.length - 1; ++i) {
        log.debug('\t' + lines[i]);
      }
      goQuitApplication();
    }
  }
};

/**
 * A listener to receive Mozmill events
 */
function MozmillMsgListener() {}
MozmillMsgListener.prototype.pass = function(obj) {
  log.debug('MOZMILL pass ' + JSON.stringify(obj) + '\n');
};
MozmillMsgListener.prototype.fail = function(obj) {
  // TODO Should this cause an error?
  log.warning('MOZMILL fail ' + JSON.stringify(obj) + '\n');
};
MozmillMsgListener.prototype.log = function(obj) {
  log.debug('MOZMILL log ' + JSON.stringify(obj) + '\n');
};

// register load listener for command line argument handling.
window.addEventListener("load", initialize, false);
