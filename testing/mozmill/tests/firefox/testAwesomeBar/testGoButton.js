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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anthony Hughes <ahughes@mozilla.com>
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Aaron Train <atrain@mozilla.com>
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
const RELATIVE_ROOT = '../../shared-modules';
const MODULE_REQUIRES = ['ToolbarAPI', 'UtilsAPI'];

const LOCAL_TEST_FOLDER = collector.addHttpResource('../test-files/');
const LOCAL_TEST_PAGES = [
  LOCAL_TEST_FOLDER + 'layout/mozilla.html',
  LOCAL_TEST_FOLDER + 'layout/mozilla_mission.html'
];

var setupModule = function() {
  controller = mozmill.getBrowserController();
  locationBar = new ToolbarAPI.locationBar(controller);

  goButton = locationBar.getElement({type: "goButton"});
}

/**
 * Test to make sure the GO button only appears while typing.
 */
var testGoButtonOnTypeOnly = function() {
  // Start from a local page
  controller.open(LOCAL_TEST_PAGES[0]);
  controller.waitForPageLoad();

  // Verify GO button is hidden
  UtilsAPI.assertElementVisible(controller, goButton, false);

  // Typing a single character should show the GO button
  locationBar.focus({type: "shortcut"});
  locationBar.type("w");
  UtilsAPI.assertElementVisible(controller, goButton, true);

  // Removing content and focus should hide the Go button
  locationBar.clear();
  controller.keypress(locationBar.urlbar, "VK_ESCAPE", {});
  UtilsAPI.assertElementVisible(controller, goButton, false);
}

/**
 * Test clicking location bar, typing a URL and clicking the GO button
 */
var testClickLocationBarAndGo = function()
{

  // Start from a local page
  controller.open(LOCAL_TEST_PAGES[0]);
  controller.waitForPageLoad();

  // Focus and type a URL; a second local page into the location bar
  locationBar.focus({type: "shortcut"});
  locationBar.type(LOCAL_TEST_PAGES[1]);

  // Click the GO button
  controller.click(goButton);
  controller.waitForPageLoad();

  // Check if an element with an id of 'organization' exists and the Go button is hidden
  var pageElement = new elementslib.ID(controller.tabs.activeTab, "organization");
  controller.assertNode(pageElement);
  UtilsAPI.assertElementVisible(controller, goButton, false);

  // Check if the URL bar matches the expected domain name
  controller.assertValue(locationBar.urlbar, LOCAL_TEST_PAGES[1]);
}

/**
 * Map test functions to litmus tests
 */
// testClickLocationBarAndGo.meta = {litmusids : [7957]};
