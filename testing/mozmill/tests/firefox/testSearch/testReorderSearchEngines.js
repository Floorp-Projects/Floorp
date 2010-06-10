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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
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
 * ***** END LICENSE BLOCK ***** */

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['SearchAPI'];

const gDelay   = 0;
const gTimeout = 5000;

// Global variable to share engine names
var gSharedData = {preEngines: [ ], postEngines: [ ]};

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  search = new SearchAPI.searchBar(controller);
}

var teardownModule = function(module)
{
  search.restoreDefaultEngines();
}

/**
 * Manage search engine (Reordering)
 */
var testReorderEngines = function()
{
  // Reorder the search engines a bit
  search.openEngineManager(reorderEngines);

  // Reopen the dialog to retrieve the current sorting
  search.openEngineManager(retrieveEngines);

  // Check the if the engines were sorted correctly in the manager
  controller.assertJS("subject.preEngines[0].name == subject.postEngines[2].name",
                      {preEngines: gSharedData.preEngines, postEngines: gSharedData.postEngines});
  controller.assertJS("subject.preEngines[1].name == subject.postEngines[1].name",
                      {preEngines: gSharedData.preEngines, postEngines: gSharedData.postEngines});
  controller.assertJS("subject.preEngines[2].name == subject.postEngines[0].name",
                      {preEngines: gSharedData.preEngines, postEngines: gSharedData.postEngines});
  controller.assertJS("subject.preEngines[subject.length - 1].name == subject.postEngines[subject.length - 2].name",
                      {preEngines: gSharedData.preEngines, postEngines: gSharedData.postEngines,
                       length: gSharedData.preEngines.length});

  // XXX: For now sleep 0ms to get the correct sorting order returned
  controller.sleep(0);

  // Check the ordering in the drop down menu
  var engines = search.visibleEngines;
  for (var ii = 0; ii < engines.length; ii++) {
    controller.assertJS("subject.visibleEngine.name == subject.expectedEngine.name",
                        {visibleEngine: engines[ii], expectedEngine: gSharedData.postEngines[ii]});
  }
}

/**
 * Reorder the search engines a bit
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var reorderEngines = function(controller)
{
  var manager = new SearchAPI.engineManager(controller);
  var engines = manager.engines;

  // Move two of the engines down
  manager.moveDownEngine(engines[0].name); // [2-1-3]
  manager.controller.sleep(gDelay);
  manager.moveDownEngine(engines[0].name); // [2-3-1]
  manager.controller.sleep(gDelay);
  manager.moveDownEngine(engines[1].name); // [3-2-1]
  manager.controller.sleep(gDelay);

  // Move one engine up
  manager.moveUpEngine(engines[engines.length - 1].name);
  manager.controller.sleep(gDelay);

  // Save initial state
  gSharedData.preEngines = engines;

  manager.close(true);
}

/**
 * Get the new search order of the engines
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var retrieveEngines = function(controller)
{
  var manager = new SearchAPI.engineManager(controller);

  // Save current state
  gSharedData.postEngines = manager.engines;

  manager.close(true);
}
