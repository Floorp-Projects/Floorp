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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tracy Walker <twalker@mozilla.com>
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
 * ***** END LICENSE BLOCK *****/

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PlacesAPI', 'ToolbarAPI'];

const gTimeout = 1000;
const gDelay = 100;

var setupModule = function(module) 
{
  module.controller = mozmill.getBrowserController();
  module.locationBar =  new ToolbarAPI.locationBar(controller);
  
  // Clear complete history so we don't get interference from previous entries
  try {
    PlacesAPI.historyService.removeAllPages();
  } catch (ex) {}
}

/**
 * Check Escape key functionality during auto-complete process
 */
var testEscape = function()
{
  var websites = ['http://www.google.com/', 'http://www.mozilla.org'];

  // Open some pages to set up the test environment
  for each (website in websites) {
    locationBar.loadURL(website);
    controller.waitForPageLoad();
  }

  // wait for 4 seconds to work around Firefox LAZY ADD of items to the db
  controller.sleep(4000);

  var testString = "google";
  
  // Focus the locationbar and delete any contents there
  locationBar.clear();

  // Use type and sleep on each letter to allow the autocomplete to populate with results. 
  for (var i = 0; i < testString.length; i++) {
    locationBar.type(testString[i]);
    controller.sleep(gDelay);
  }

  // confirm that google is in the locationbar and the awesomecomplete list is open
  controller.assertJS("subject.contains('" + testString + "') == true", locationBar);
  controller.assertJS("subject.autoCompleteResults.isOpened == true", locationBar);

  // After first Escape, confirm that google is in the locationbar and awesomecomplete list is closed
  controller.keypress(locationBar.urlbar, 'VK_ESCAPE', {});
  controller.assertJS("subject.contains('" + testString + "') == true", locationBar);
  controller.assertJS("subject.autoCompleteResults.isOpened == false", locationBar);
  
  // After second Escape, confirm the locationbar returns to the current page url
  controller.keypress(locationBar.urlbar, 'VK_ESCAPE', {});
  controller.assertJS("subject.contains('" + websites[1] + "') == true", locationBar);
}

/**
 * Map test function to litmus test
 */
// testEscape.meta = {litmusids : [8693]};
