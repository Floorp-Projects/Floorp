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
        suite = new TestSuite(obj.tests);
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
