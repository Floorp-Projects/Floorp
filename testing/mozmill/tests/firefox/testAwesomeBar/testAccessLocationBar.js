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
 *   Tracy Walker <twalker@mozilla.com>
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
 * **** END LICENSE BLOCK *****/

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PlacesAPI', 'ToolbarAPI'];

const gTimeout = 5000;
const gDelay = 100;

const websites = ['http://www.google.com/',
                  'http://www.mozilla.org/',
                  'http://www.getpersonas.com/',
                  'about:blank'];

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
  locationBar = new ToolbarAPI.locationBar(controller);

  // Clear complete history so we don't get interference from previous entries
  try {
    PlacesAPI.historyService.removeAllPages();
  } catch (ex) {}
}

/**
 * Check acces to the location bar drop down list via autocomplete
 */
var testAccessLocationBarHistory = function()
{
  // Open a few different sites to create a small history (about:blank doesn't
  // appear in history and clears the page for clean test arena
  for each (website in websites) {
    locationBar.loadURL(website);
    controller.waitForPageLoad();
  }

  // Wait about 4s so the history gets populated
  controller.sleep(4000);

  // First - Focus the locationbar then delete any contents there
  locationBar.clear();

  // Second - Arrow down to open the autocomplete list (displays most recent visit first),
  // then arrow down again to the first entry, in this case www.getpersonas.com;
  controller.keypress(locationBar.urlbar, "VK_DOWN", {});
  controller.sleep(gDelay);
  controller.keypress(locationBar.urlbar, "VK_DOWN", {});
  controller.sleep(gDelay);

  // checks that the first item in the drop down list is selected.
  controller.waitForEval("subject.selectedIndex == 0",
                         gTimeout, 100, locationBar.autoCompleteResults);
  locationBar.contains("getpersonas");
  controller.keypress(null, "VK_RETURN", {});
  controller.waitForPageLoad();

  // Finally - Check that the personas page was loaded
  // Check for presense of Personsas image
  var personasImage = new elementslib.XPath(controller.tabs.activeTab, "/html/body/div[@id='outer-wrapper']/div[@id='inner-wrapper']/div[@id='nav']/h1/a/img");
  controller.waitForElement(personasImage, gTimeout, 100);

  // Check that getpersonas is in the url bar
  locationBar.contains("getpersonas");
}

/**
 * Map test functions to litmus tests
 */
// testAccessLocationBarHistory.meta = {litmusids : [5981]};
