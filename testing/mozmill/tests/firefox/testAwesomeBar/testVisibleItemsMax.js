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
var MODULE_REQUIRES = ['PlacesAPI', 'PrefsAPI','ToolbarAPI'];

const gTimeout = 5000;
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
 * Check Six is the maximum visible items in a match list.
 */
var testVisibleItemsMax = function()
{
  // Use preferences dialog to ensure "When Using the location bar suggest:" History and Bookmarks is selected
  PrefsAPI.openPreferencesDialog(prefDialogSuggestsCallback);

  // Load some pages to populate history.
  var websites = [
                  'http://www.google.com/',
                  'http://www.mozilla.org',
                  'http://www.mozilla.org/projects/',
                  'http://www.mozilla.org/about/history.html',
                  'http://www.mozilla.org/contribute/',
                  'http://www.mozilla.org/causes/',
                  'http://www.mozilla.org/community/',
                  'http://www.mozilla.org/about/'
                 ];

  // Open some pages to set up the test environment
  for each (website in websites) {
    locationBar.loadURL(website);
    controller.waitForPageLoad();
  }

  // wait for 4 seconds to work around Firefox LAZY ADD of items to the db
  controller.sleep(4000);

  var testString = 'll';

  // Focus the locationbar, delete any contents there
  locationBar.clear();

  // Use type and sleep on each letter to allow the autocomplete to populate with results.
  for each (letter in testString) {
    locationBar.type(letter);
    controller.sleep(gDelay);
  }

  // Get the visible results from the autocomplete list. Verify it is six
  var autoCompleteResultsList = locationBar.autoCompleteResults.getElement({type:"results"});
  controller.assertJS("subject.getNumberOfVisibleRows() == 6", autoCompleteResultsList.getNode());
}

/**
 * Set matching of the location bar to "History and Bookmarks"
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogSuggestsCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'panePrivacy';

  var suggests = new elementslib.ID(controller.window.document, "locationBarSuggestion");
  controller.waitForElement(suggests);
  controller.select(suggests, null, null, 0);
  controller.sleep(gDelay);

  prefDialog.close(true);
}
