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
 * ***** END LICENSE BLOCK *****/

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PlacesAPI', 'PrefsAPI', 'ToolbarAPI'];

var testSite = {url : 'https://litmus.mozilla.org/', string: 'litmus'};

const gTimeout = 5000;
const gDelay = 200;

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
  module.locationBar =  new ToolbarAPI.locationBar(controller);

  // Clear complete history so we don't get interference from previous entries
  try {
    PlacesAPI.historyService.removeAllPages();
  } catch (ex) {}
}

var teardownModule = function(module)
{
  PlacesAPI.restoreDefaultBookmarks();
}

/**
 * Check history and bookmarked (done in testStarInAutocomplete()) items appear in autocomplete list.
 */
var testSuggestHistoryAndBookmarks = function()
{
  // Use preferences dialog to select "When Using the location bar suggest:" History and Bookmarks
  PrefsAPI.openPreferencesDialog(prefDialogSuggestsCallback);

  // Open the test page
  locationBar.loadURL(testSite.url);
  controller.waitForPageLoad();

  // wait for 4 seconds to work around Firefox LAZY ADD of items to the db
  controller.sleep(4000);

  // Focus the locationbar, delete any contents there
  locationBar.clear();

  // Type in each letter of the test string to allow the autocomplete to populate with results.
  for each (letter in testSite.string) {
    locationBar.type(letter);
    controller.sleep(gDelay);
  }

  // defines the path to the first auto-complete result
  var richlistItem = locationBar.autoCompleteResults.getResult(0);

  // Get the visible results from the autocomplete list. Verify it is 1
  controller.waitForEval('subject.isOpened == true', 3000, 100, locationBar.autoCompleteResults);

  var autoCompleteResultsList = locationBar.autoCompleteResults.getElement({type:"results"});
  controller.assertJS("subject.getNumberOfVisibleRows() == 1", autoCompleteResultsList.getNode());

  // For the page title check matched text is underlined
  var entries = locationBar.autoCompleteResults.getUnderlinedText(richlistItem, "title");
  for each (entry in entries) {
    controller.assertJS("subject.enteredTitle == subject.underlinedTitle",
                        {enteredTitle: testSite.string, underlinedTitle: entry.toLowerCase()});
  }
}

/**
 * Check a star appears in autocomplete list for a bookmarked page.
 */
var testStarInAutocomplete = function()
{
  // Bookmark the test page via bookmarks menu
  controller.click(new elementslib.Elem(controller.menus.bookmarksMenu.menu_bookmarkThisPage));

  // editBookmarksPanel is loaded lazily. Wait until overlay for StarUI has been loaded, then close the dialog
  controller.waitForEval("subject._overlayLoaded == true", gTimeout, gDelay, controller.window.top.StarUI);
  var doneButton = new elementslib.ID(controller.window.document, "editBookmarkPanelDoneButton");
  controller.click(doneButton);

  // defines the path to the first auto-complete result
  var richlistItem = locationBar.autoCompleteResults.getResult(0);

  // Clear history
  try {
    PlacesAPI.historyService.removeAllPages();
  } catch (ex) {}

  // Focus the locationbar, delete any contents there
  locationBar.clear();

  // Type in each letter of the test string to allow the autocomplete to populate with results.
  for each (letter in testSite.string) {
    locationBar.type(letter);
    controller.sleep(gDelay);
  }

  // For the page title check matched text is underlined
  controller.waitForEval('subject.isOpened == true', 3000, 100, locationBar.autoCompleteResults);

  var entries = locationBar.autoCompleteResults.getUnderlinedText(richlistItem, "title");
  for each (entry in entries) {
    controller.assertJS("subject.enteredTitle == subject.underlinedTitle",
                        {enteredTitle: testSite.string, underlinedTitle: entry.toLowerCase()});
  }

  // For icons, check that the bookmark star is present
  controller.assertJS("subject.isItemBookmarked == true",
                      {isItemBookmarked: richlistItem.getNode().getAttribute('type') == 'bookmark'});
}

/**
 * Set suggests in the location bar to "History and Bookmarks"
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
